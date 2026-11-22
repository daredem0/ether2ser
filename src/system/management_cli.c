#include "system/management_cli.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drivers/gpio_control.h"
#include "system/management_catalog.h"
#include "system/operating_mode.h"

#define MANAGEMENT_CLI_BUFFER_SIZE (MANAGEMENT_CLI_MAX_LINE + 1U)
#define IPV4_COMPONENT_COUNT 4U

static bool parse_u32(const char* text, uint32_t* value)
{
    if (text == NULL || value == NULL || text[0] == '\0')
    {
        return false;
    }
    errno = 0;
    char* end = NULL;
    const unsigned long parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed > UINT32_MAX)
    {
        return false;
    }
    *value = (uint32_t)parsed;
    return true;
}

static bool parse_ipv4(const char* text, uint8_t address[IPV4_COMPONENT_COUNT])
{
    if (text == NULL || address == NULL)
    {
        return false;
    }

    const char* cursor = text;
    for (size_t i = 0U; i < IPV4_COMPONENT_COUNT; i++)
    {
        errno = 0;
        char* end = NULL;
        const unsigned long part = strtoul(cursor, &end, 10);
        if (errno != 0 || end == cursor || part > UINT8_MAX)
        {
            return false;
        }
        if (i + 1U < IPV4_COMPONENT_COUNT)
        {
            if (*end != '.')
            {
                return false;
            }
            cursor = end + 1;
        }
        else if (*end != '\0')
        {
            return false;
        }
        address[i] = (uint8_t)part;
    }
    return true;
}

static bool parse_ipv4_cidr(char* text, management_ipv4_cidr_t* cidr)
{
    if (text == NULL || cidr == NULL)
    {
        return false;
    }
    char* slash = strchr(text, '/');
    if (slash == NULL)
    {
        return false;
    }
    *slash = '\0';
    uint32_t prefix = 0U;
    if (!parse_ipv4(text, cidr->address) || !parse_u32(slash + 1, &prefix) || prefix > 32U)
    {
        return false;
    }
    cidr->prefix_length = (uint8_t)prefix;
    return true;
}

static bool parse_on_off(const char* text, bool* value)
{
    if (strcmp(text, "on") == 0)
    {
        *value = true;
        return true;
    }
    if (strcmp(text, "off") == 0)
    {
        *value = false;
        return true;
    }
    return false;
}

static bool parse_invert_mask(char* text, uint32_t* mask)
{
    static const char* names[] = {"txd", "tck", "cts", "rts", "dtr", "rxd", "rck", "dcd"};
    *mask = 0U;
    if (strcmp(text, "none") == 0)
    {
        return true;
    }

    char* save = NULL;
    for (char* token = strtok_r(text, ",", &save); token != NULL;
         token = strtok_r(NULL, ",", &save))
    {
        bool found = false;
        for (size_t i = 0U; i < (sizeof(names) / sizeof(names[0])); i++)
        {
            if (strcmp(token, names[i]) == 0)
            {
                *mask |= 1UL << i;
                found = true;
                break;
            }
        }
        if (!found)
        {
            return false;
        }
    }
    return true;
}

static management_result_t resolve_path(const char* path, management_request_t* request,
                                        const char** canonical_path)
{
    if (strncmp(path, "gpio.", 5U) == 0)
    {
        const gpio_signal_info_t* signal = gpio_control_find_signal(path + 5U);
        if (signal == NULL)
        {
            return MANAGEMENT_RESULT_NOT_FOUND;
        }
        request->resource               = MANAGEMENT_RESOURCE_GPIO;
        request->value.kind             = MANAGEMENT_VALUE_GPIO;
        request->value.data.gpio.signal = signal->signal;
        *canonical_path                 = "gpio";
        return MANAGEMENT_RESULT_OK;
    }
    if (strncmp(path, "pins.", 5U) == 0)
    {
        const gpio_signal_info_t* signal = gpio_control_find_signal(path + 5U);
        if (signal == NULL)
        {
            return MANAGEMENT_RESULT_NOT_FOUND;
        }
        request->resource               = MANAGEMENT_RESOURCE_PIN;
        request->value.kind             = MANAGEMENT_VALUE_GPIO;
        request->value.data.gpio.signal = signal->signal;
        *canonical_path                 = "pin";
        return MANAGEMENT_RESULT_OK;
    }

    const management_resource_descriptor_t* descriptor = management_catalog_find(path);
    if (descriptor == NULL)
    {
        return MANAGEMENT_RESULT_NOT_FOUND;
    }
    request->resource = descriptor->resource;
    *canonical_path   = descriptor->path;
    return MANAGEMENT_RESULT_OK;
}

static management_result_t parse_set_value(char* text, management_request_t* request)
{
    uint32_t parsed = 0U;
    switch (request->resource)
    {
    case MANAGEMENT_RESOURCE_SYSTEM_MODE:
        request->value.kind = MANAGEMENT_VALUE_MODE;
        if (strcmp(text, "bridge") == 0)
        {
            request->value.data.mode = OPERATING_MODE_BRIDGE;
            return MANAGEMENT_RESULT_OK;
        }
        if (strcmp(text, "gpio") == 0)
        {
            request->value.data.mode = OPERATING_MODE_GPIO;
            return MANAGEMENT_RESULT_OK;
        }
        break;
    case MANAGEMENT_RESOURCE_NET_IP_LOCAL:
        request->value.kind = MANAGEMENT_VALUE_IPV4_CIDR;
        return parse_ipv4_cidr(text, &request->value.data.ipv4_cidr)
                   ? MANAGEMENT_RESULT_OK
                   : MANAGEMENT_RESULT_INVALID_VALUE;
    case MANAGEMENT_RESOURCE_NET_IP_REMOTE:
    case MANAGEMENT_RESOURCE_NET_IP_GATEWAY:
        request->value.kind = MANAGEMENT_VALUE_IPV4;
        return parse_ipv4(text, request->value.data.ipv4) ? MANAGEMENT_RESULT_OK
                                                          : MANAGEMENT_RESULT_INVALID_VALUE;
    case MANAGEMENT_RESOURCE_NET_UDP_PORT_LOCAL:
    case MANAGEMENT_RESOURCE_NET_UDP_PORT_REMOTE:
        request->value.kind = MANAGEMENT_VALUE_U16;
        if (parse_u32(text, &parsed) && parsed > 0U && parsed <= UINT16_MAX)
        {
            request->value.data.u16 = (uint16_t)parsed;
            return MANAGEMENT_RESULT_OK;
        }
        break;
    case MANAGEMENT_RESOURCE_V24_BAUDRATE:
        request->value.kind = MANAGEMENT_VALUE_U32;
        if (parse_u32(text, &parsed))
        {
            request->value.data.u32 = parsed;
            return MANAGEMENT_RESULT_OK;
        }
        break;
    case MANAGEMENT_RESOURCE_V24_TXCLOCK:
        request->value.kind = MANAGEMENT_VALUE_BOOL;
        if (strcmp(text, "internal") == 0 || strcmp(text, "external") == 0)
        {
            request->value.data.boolean = strcmp(text, "external") == 0;
            return MANAGEMENT_RESULT_OK;
        }
        break;
    case MANAGEMENT_RESOURCE_V24_CTS_RESTART:
        request->value.kind = MANAGEMENT_VALUE_BOOL;
        return parse_on_off(text, &request->value.data.boolean)
                   ? MANAGEMENT_RESULT_OK
                   : MANAGEMENT_RESULT_INVALID_VALUE;
    case MANAGEMENT_RESOURCE_V24_INVERT:
        request->value.kind = MANAGEMENT_VALUE_SIGNAL_MASK;
        return parse_invert_mask(text, &request->value.data.u32)
                   ? MANAGEMENT_RESULT_OK
                   : MANAGEMENT_RESULT_INVALID_VALUE;
    case MANAGEMENT_RESOURCE_SYSTEM_LOGLEVEL:
        request->value.kind = MANAGEMENT_VALUE_U32;
        if (strcmp(text, "error") == 0)
        {
            request->value.data.u32 = 1U;
            return MANAGEMENT_RESULT_OK;
        }
        if (strcmp(text, "info") == 0)
        {
            request->value.data.u32 = 2U;
            return MANAGEMENT_RESULT_OK;
        }
        if (strcmp(text, "debug") == 0)
        {
            request->value.data.u32 = 3U;
            return MANAGEMENT_RESULT_OK;
        }
        if (strcmp(text, "trace") == 0)
        {
            request->value.data.u32 = 4U;
            return MANAGEMENT_RESULT_OK;
        }
        break;
    case MANAGEMENT_RESOURCE_GPIO:
        if (strcmp(text, "0") == 0 || strcmp(text, "1") == 0)
        {
            request->value.kind            = MANAGEMENT_VALUE_GPIO;
            request->value.data.gpio.level = text[0] == '1';
            return MANAGEMENT_RESULT_OK;
        }
        break;
    default:
        return MANAGEMENT_RESULT_READ_ONLY;
    }
    return MANAGEMENT_RESULT_INVALID_VALUE;
}

management_result_t management_cli_parse(const char* line, management_request_t* request,
                                         const char** canonical_path)
{
    if (line == NULL || request == NULL || canonical_path == NULL)
    {
        return MANAGEMENT_RESULT_INTERNAL_ERROR;
    }
    const size_t length = strnlen(line, MANAGEMENT_CLI_BUFFER_SIZE);
    if (length > MANAGEMENT_CLI_MAX_LINE)
    {
        return MANAGEMENT_RESULT_TOO_LARGE;
    }
    if (length == 0U)
    {
        return MANAGEMENT_RESULT_INVALID_VALUE;
    }

    char buffer[MANAGEMENT_CLI_BUFFER_SIZE];
    memcpy(buffer, line, length + 1U);
    memset(request, 0, sizeof(*request));
    *canonical_path = "";

    char* save = NULL;
    char* verb = strtok_r(buffer, " ", &save);
    char* path = strtok_r(NULL, " ", &save);
    char* value = strtok_r(NULL, " ", &save);
    char* extra = strtok_r(NULL, " ", &save);
    if (verb == NULL || extra != NULL)
    {
        return MANAGEMENT_RESULT_INVALID_VALUE;
    }

    if (strcmp(verb, "save") == 0 || strcmp(verb, "reboot") == 0 || strcmp(verb, "wipe") == 0)
    {
        request->method = MANAGEMENT_METHOD_ACTION;
        if (strcmp(verb, "save") == 0 && path == NULL)
        {
            request->resource = MANAGEMENT_RESOURCE_ACTION_SAVE;
            *canonical_path   = "actions.save";
            return MANAGEMENT_RESULT_OK;
        }
        if (strcmp(verb, "reboot") == 0 && path == NULL)
        {
            request->resource = MANAGEMENT_RESOURCE_ACTION_REBOOT;
            *canonical_path   = "actions.reboot";
            return MANAGEMENT_RESULT_OK;
        }
        if (strcmp(verb, "wipe") == 0 && path != NULL && value == NULL &&
            strcmp(path, "wipe") == 0)
        {
            request->resource        = MANAGEMENT_RESOURCE_ACTION_WIPE;
            request->value.kind      = MANAGEMENT_VALUE_TEXT;
            request->value.data.text = "wipe";
            *canonical_path          = "actions.wipe";
            return MANAGEMENT_RESULT_OK;
        }
        return MANAGEMENT_RESULT_INVALID_VALUE;
    }

    if (strcmp(verb, "help") == 0)
    {
        if (value != NULL)
        {
            return MANAGEMENT_RESULT_INVALID_VALUE;
        }
        request->method   = MANAGEMENT_METHOD_GET;
        request->resource = MANAGEMENT_RESOURCE_DISCOVERY;
        if (path != NULL)
        {
            const management_resource_descriptor_t* descriptor = management_catalog_find(path);
            if (descriptor == NULL)
            {
                return MANAGEMENT_RESULT_NOT_FOUND;
            }
            request->value.kind      = MANAGEMENT_VALUE_TEXT;
            request->value.data.text = descriptor->path;
        }
        *canonical_path = "help";
        return MANAGEMENT_RESULT_OK;
    }

    if ((strcmp(verb, "get") != 0 && strcmp(verb, "set") != 0) || path == NULL)
    {
        return MANAGEMENT_RESULT_NOT_FOUND;
    }
    request->method = strcmp(verb, "get") == 0 ? MANAGEMENT_METHOD_GET : MANAGEMENT_METHOD_SET;
    management_result_t result = resolve_path(path, request, canonical_path);
    if (result != MANAGEMENT_RESULT_OK)
    {
        return result;
    }
    if (request->method == MANAGEMENT_METHOD_GET)
    {
        return value == NULL ? MANAGEMENT_RESULT_OK : MANAGEMENT_RESULT_INVALID_VALUE;
    }
    if (value == NULL)
    {
        return MANAGEMENT_RESULT_INVALID_VALUE;
    }
    return parse_set_value(value, request);
}

static int write_human_translation(char* output, size_t capacity,
                                   const char* format, ...)
{
    va_list args;
    va_start(args, format);
    const int written = vsnprintf(output, capacity, format, args);
    va_end(args);
    return ((written < 0) || ((size_t)written >= capacity)) ? -1 : 1;
}

static int translate_human_command(const char* line, char* output, size_t capacity)
{
    char buffer[MANAGEMENT_CLI_BUFFER_SIZE];
    const size_t length = strnlen(line, sizeof(buffer));
    if (length >= sizeof(buffer))
    {
        return -1;
    }
    memcpy(buffer, line, length + 1U);

    char* tokens[5] = {0};
    size_t count = 0U;
    char* save = NULL;
    for (char* token = strtok_r(buffer, " ", &save);
         token != NULL && count < (sizeof(tokens) / sizeof(tokens[0]));
         token = strtok_r(NULL, " ", &save))
    {
        tokens[count++] = token;
    }
    if ((count == 0U) || (count == (sizeof(tokens) / sizeof(tokens[0]))))
    {
        return 0;
    }

    if (count == 1U)
    {
        if (strcmp(tokens[0], "status") == 0)
        {
            return write_human_translation(output, capacity, "get status");
        }
        if (strcmp(tokens[0], "mem") == 0)
        {
            return write_human_translation(output, capacity, "get memory");
        }
        if ((strcmp(tokens[0], "net") == 0) ||
            (strcmp(tokens[0], "config") == 0) ||
            (strcmp(tokens[0], "v24") == 0) ||
            (strcmp(tokens[0], "pins") == 0))
        {
            return write_human_translation(output, capacity, "get %s", tokens[0]);
        }
        if (strcmp(tokens[0], "mode") == 0)
        {
            return write_human_translation(output, capacity, "get system.mode");
        }
        if (strcmp(tokens[0], "loglevel") == 0)
        {
            return write_human_translation(output, capacity, "get system.loglevel");
        }
        return 0;
    }

    if ((count == 2U) && (strcmp(tokens[0], "help") == 0))
    {
        if (strcmp(tokens[1], "mode") == 0)
        {
            return write_human_translation(output, capacity, "help system.mode");
        }
        if (strcmp(tokens[1], "loglevel") == 0)
        {
            return write_human_translation(output, capacity, "help system.loglevel");
        }
        if (strcmp(tokens[1], "mem") == 0)
        {
            return write_human_translation(output, capacity, "help memory");
        }
        return 0;
    }
    if ((count == 3U) && (strcmp(tokens[0], "help") == 0) &&
        ((strcmp(tokens[1], "net") == 0) ||
         (strcmp(tokens[1], "v24") == 0)))
    {
        return write_human_translation(output, capacity, "help %s.%s",
                                       tokens[1], tokens[2]);
    }
    if ((count == 2U) && (strcmp(tokens[0], "pininfo") == 0))
    {
        return write_human_translation(output, capacity, "get pins.%s", tokens[1]);
    }
    if ((count == 2U) && (strcmp(tokens[0], "mode") == 0))
    {
        return write_human_translation(output, capacity, "set system.mode %s", tokens[1]);
    }
    if ((count == 2U) && (strcmp(tokens[0], "loglevel") == 0))
    {
        return write_human_translation(output, capacity, "set system.loglevel %s",
                                       tokens[1]);
    }

    const bool is_get = strcmp(tokens[0], "get") == 0;
    const bool is_set = strcmp(tokens[0], "set") == 0;
    if (!is_get && !is_set)
    {
        return 0;
    }
    if ((count == 2U) && (strcmp(tokens[1], "loglevel") == 0))
    {
        return is_get
                   ? write_human_translation(output, capacity, "get system.loglevel")
                   : 0;
    }
    if ((count == 3U) && is_set && (strcmp(tokens[1], "loglevel") == 0))
    {
        return write_human_translation(output, capacity, "set system.loglevel %s",
                                       tokens[2]);
    }
    if ((count == 3U) && is_get &&
        ((strcmp(tokens[1], "gpio") == 0) ||
         (strcmp(tokens[1], "net") == 0) ||
         (strcmp(tokens[1], "v24") == 0)))
    {
        return write_human_translation(output, capacity, "get %s.%s",
                                       tokens[1], tokens[2]);
    }
    if ((count == 4U) && is_set &&
        ((strcmp(tokens[1], "gpio") == 0) ||
         (strcmp(tokens[1], "net") == 0) ||
         (strcmp(tokens[1], "v24") == 0)))
    {
        const char* value = tokens[3];
        if ((strcmp(tokens[1], "v24") == 0) &&
            (strcmp(tokens[2], "txclock") == 0))
        {
            if (strcmp(value, "0") == 0)
            {
                value = "internal";
            }
            else if (strcmp(value, "1") == 0)
            {
                value = "external";
            }
        }
        else if ((strcmp(tokens[1], "v24") == 0) &&
                 (strcmp(tokens[2], "ctsrestart") == 0))
        {
            if (strcmp(value, "0") == 0)
            {
                value = "off";
            }
            else if (strcmp(value, "1") == 0)
            {
                value = "on";
            }
        }
        return write_human_translation(output, capacity, "set %s.%s %s",
                                       tokens[1], tokens[2], value);
    }
    return 0;
}

management_result_t management_cli_parse_human(const char* line,
                                               management_request_t* request,
                                               const char** canonical_path)
{
    if (line == NULL)
    {
        return MANAGEMENT_RESULT_INTERNAL_ERROR;
    }
    char translated[MANAGEMENT_CLI_BUFFER_SIZE];
    const int translation =
        translate_human_command(line, translated, sizeof(translated));
    if (translation < 0)
    {
        return MANAGEMENT_RESULT_TOO_LARGE;
    }
    return management_cli_parse(translation > 0 ? translated : line,
                                request, canonical_path);
}

const char* management_cli_help_overview(void)
{
    return "\r\n"
           "Commands:\r\n"
           "  help [topic]                     Show this menu or topic help\r\n"
           "  status                           Show bridge health and counters\r\n"
           "  mem                              Show RAM and flash usage\r\n"
           "  net                              Show network configuration\r\n"
           "  config                           Show complete configuration\r\n"
           "  v24                              Show V.24 configuration\r\n"
           "  mode [bridge|gpio]               Show or change operating mode\r\n"
           "  pins                             List GPIO pins\r\n"
           "  pininfo <signal>                 Show one GPIO pin\r\n"
           "\r\n"
           "Read-only views:\r\n"
           "  status, mem, net, config, v24, pins\r\n"
           "\r\n"
           "Configuration:\r\n"
           "  get|set gpio <signal> [0|1]\r\n"
           "  get|set net <ip.local|ip.remote|ip.gateway> [value]\r\n"
           "  get|set net <udp.port.local|udp.port.remote> [port]\r\n"
           "  get|set v24 <baudrate|txclock|invert|ctsrestart> [value]\r\n"
           "  get|set loglevel [error|info|debug|trace]\r\n"
           "  mode <bridge|gpio>\r\n"
           "\r\n"
           "Actions:\r\n"
           "  save                             Persist runtime settings\r\n"
           "  reboot                           Reboot the device\r\n"
           "  wipe wipe                        Erase persisted configuration\r\n"
           "\r\n"
           "Pins:\r\n"
           "  outputs : txd, rts, dtr, tx_active, tck\r\n"
           "  inputs  : rxd, cts, dsr, dcd, rck, xck\r\n"
           "  reserved: w550_reset\r\n"
           "\r\n"
           "Baudrates:\r\n"
           "  1200, 2400, 4800, 9600, 16000, 19200, 38400, 57600, 115200\r\n"
           "\r\n"
           "Examples:\r\n"
           "  set net ip.local 192.168.29.20/24\r\n"
           "  set v24 baudrate 16000\r\n"
           "  set gpio rts 1\r\n"
           "\r\n"
           "Use 'help <resource>' for detailed syntax.\r\n";
}

const char* management_cli_help_usage(management_resource_t resource)
{
    switch (resource)
    {
    case MANAGEMENT_RESOURCE_DISCOVERY:
        return "  help [path]\r\n";
    case MANAGEMENT_RESOURCE_STATUS:
        return "  status\r\n";
    case MANAGEMENT_RESOURCE_MEMORY:
        return "  mem\r\n";
    case MANAGEMENT_RESOURCE_CONFIG:
        return "  config\r\n";
    case MANAGEMENT_RESOURCE_NET:
        return "  net\r\n";
    case MANAGEMENT_RESOURCE_NET_IP_LOCAL:
        return "  get net ip.local\r\n"
               "  set net ip.local <address/prefix>\r\n";
    case MANAGEMENT_RESOURCE_NET_IP_REMOTE:
        return "  get net ip.remote\r\n"
               "  set net ip.remote <address>\r\n";
    case MANAGEMENT_RESOURCE_NET_IP_GATEWAY:
        return "  get net ip.gateway\r\n"
               "  set net ip.gateway <address>\r\n";
    case MANAGEMENT_RESOURCE_NET_UDP_PORT_LOCAL:
        return "  get net udp.port.local\r\n"
               "  set net udp.port.local <1..65535>\r\n";
    case MANAGEMENT_RESOURCE_NET_UDP_PORT_REMOTE:
        return "  get net udp.port.remote\r\n"
               "  set net udp.port.remote <1..65535>\r\n";
    case MANAGEMENT_RESOURCE_V24:
        return "  v24\r\n";
    case MANAGEMENT_RESOURCE_V24_BAUDRATE:
        return "  get v24 baudrate\r\n"
               "  set v24 baudrate <1200|2400|4800|9600|16000|19200|38400|57600|115200>\r\n";
    case MANAGEMENT_RESOURCE_V24_TXCLOCK:
        return "  get v24 txclock\r\n"
               "  set v24 txclock <internal|external>\r\n";
    case MANAGEMENT_RESOURCE_V24_INVERT:
        return "  get v24 invert\r\n"
               "  set v24 invert <none|signal,...>\r\n";
    case MANAGEMENT_RESOURCE_V24_CTS_RESTART:
        return "  get v24 ctsrestart\r\n"
               "  set v24 ctsrestart <on|off>\r\n";
    case MANAGEMENT_RESOURCE_SYSTEM_LOGLEVEL:
        return "  get loglevel\r\n"
               "  set loglevel <error|info|debug|trace>\r\n";
    case MANAGEMENT_RESOURCE_SYSTEM_MODE:
        return "  mode\r\n"
               "  mode <bridge|gpio>\r\n";
    case MANAGEMENT_RESOURCE_PINS:
        return "  pins\r\n";
    case MANAGEMENT_RESOURCE_PIN:
        return "  pininfo <signal>\r\n";
    case MANAGEMENT_RESOURCE_GPIO:
        return "  get gpio <signal>\r\n"
               "  set gpio <signal> <0|1>  (gpio mode, outputs only)\r\n";
    case MANAGEMENT_RESOURCE_ACTION_SAVE:
        return "  save\r\n";
    case MANAGEMENT_RESOURCE_ACTION_WIPE:
        return "  wipe wipe\r\n";
    case MANAGEMENT_RESOURCE_ACTION_REBOOT:
        return "  reboot\r\n";
    default:
        return "";
    }
}

static bool append_text(char* output, size_t capacity, size_t* used, const char* format, ...)
{
    if (*used >= capacity)
    {
        return false;
    }
    va_list args;
    va_start(args, format);
    const int written = vsnprintf(output + *used, capacity - *used, format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= capacity - *used)
    {
        return false;
    }
    *used += (size_t)written;
    return true;
}

bool management_cli_format_response(uint32_t sequence, const char* canonical_path,
                                    const management_response_t* response,
                                    char* output, size_t output_capacity)
{
    if (canonical_path == NULL || response == NULL || output == NULL || output_capacity == 0U)
    {
        return false;
    }
    size_t used = 0U;
    output[0] = '\0';
    if (response->result != MANAGEMENT_RESULT_OK &&
        response->result != MANAGEMENT_RESULT_ACCEPTED)
    {
        return append_text(output, output_capacity, &used, "@response %lu end error %s\r\n",
                           (unsigned long)sequence, management_result_name(response->result));
    }
    if (!append_text(output, output_capacity, &used, "@response %lu begin %s\r\n",
                     (unsigned long)sequence, canonical_path))
    {
        return false;
    }

    switch (response->value.kind)
    {
    case MANAGEMENT_VALUE_MODE:
        if (!append_text(output, output_capacity, &used, "%s=%s\r\n", canonical_path,
                         operating_mode_name(response->value.data.mode)))
        {
            return false;
        }
        break;
    case MANAGEMENT_VALUE_BOOL:
        if (!append_text(output, output_capacity, &used, "%s=%s\r\n", canonical_path,
                         response->value.data.boolean ? "on" : "off"))
        {
            return false;
        }
        break;
    case MANAGEMENT_VALUE_U16:
        if (!append_text(output, output_capacity, &used, "%s=%u\r\n", canonical_path,
                         (unsigned)response->value.data.u16))
        {
            return false;
        }
        break;
    case MANAGEMENT_VALUE_U32:
    case MANAGEMENT_VALUE_SIGNAL_MASK:
        if (!append_text(output, output_capacity, &used, "%s=%lu\r\n", canonical_path,
                         (unsigned long)response->value.data.u32))
        {
            return false;
        }
        break;
    case MANAGEMENT_VALUE_GPIO:
    {
        const gpio_signal_info_t* signal =
            gpio_control_signal_at((size_t)response->value.data.gpio.signal);
        if (signal == NULL ||
            !append_text(output, output_capacity, &used, "gpio.%s=%u\r\n", signal->name,
                         response->value.data.gpio.level ? 1U : 0U))
        {
            return false;
        }
        break;
    }
    case MANAGEMENT_VALUE_NONE:
    case MANAGEMENT_VALUE_IPV4:
    case MANAGEMENT_VALUE_IPV4_CIDR:
    case MANAGEMENT_VALUE_TEXT:
    default:
        break;
    }
    return append_text(output, output_capacity, &used, "@response %lu end %s\r\n",
                       (unsigned long)sequence, management_result_name(response->result));
}
