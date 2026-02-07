/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/cli_commands.c
 * Purpose: CLI command handlers and dispatch.
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "cli_commands.h"

// Standard library headers
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Library Headers
#include "hardware/gpio.h"
#include "hardware/structs/io_bank0.h"
#include "hardware/watchdog.h"
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"

// Project Headers
#include "drivers/v24_config.h"
#include "platform/pinmap.h"
#include "system/baudrate_monitor.h"
#include "system/cli_parser.h"
#include "system/common.h"
#include "system/error.h"
#include "system/event_queue.h"
// Generated headers

#define MAX_CMD_BUFFER_LEN 16
#define MAX_PIN_NAME_LEN 16
#define MAX_ARG_BUFFER_LEN 64

// Command handler type
typedef void (*cmd_handler_t)(const char* args);

typedef struct
{
    const char*   name;
    cmd_handler_t handler;
    const char*   help;
} command_t;

typedef void (*category_set_handler_t)(const char* args);
typedef void (*category_get_handler_t)(const char* args);

typedef struct
{
    const char*            name;
    category_set_handler_t set_handler;
    category_get_handler_t get_handler;
    const char*            help;
} category_t;

typedef void (*subcmd_set_handler_t)(const char* args);
typedef void (*subcmd_get_handler_t)(const char* args);

typedef struct
{
    const char*          name;
    subcmd_set_handler_t set_handler;
    subcmd_get_handler_t get_handler;
    const char*          help;
} subcmd_t;

// Forward declarations for lookup tables
static void cmd_help(const char* args);
static void cmd_status(const char* args);
static void cmd_net(const char* args);
static void cmd_set(const char* args);
static void cmd_get(const char* args);
static void cmd_pininfo(const char* args);
static void cmd_save(const char* args);
static void cmd_wipe(const char* args);
static void cmd_reboot(const char* args);
static void cat_gpio_set(const char* args);
static void cat_gpio_get(const char* args);
static void cat_net_set(const char* args);
static void cat_net_get(const char* args);
static void cat_v24_set(const char* args);
static void cat_v24_get(const char* args);
static void subcmd_set_ip_local(const char* args);
static void subcmd_get_ip_local(const char* args);
static void subcmd_set_ip_remote(const char* args);
static void subcmd_get_ip_remote(const char* args);
static void subcmd_set_ip_gateway(const char* args);
static void subcmd_get_ip_gateway(const char* args);
static void subcmd_set_udp_port_local(const char* args);
static void subcmd_get_udp_port_local(const char* args);
static void subcmd_set_udp_port_remote(const char* args);
static void subcmd_get_udp_port_remote(const char* args);
static void subcmd_set_v24_baudrate(const char* args);
static void subcmd_get_v24_baudrate(const char* args);
static void subcmd_set_v24_inverted(const char* args);
static void subcmd_get_v24_inverted(const char* args);

// Lookup tables
static const command_t commands[] = {
    {"help", cmd_help, "Show available commands"},
    {"status", cmd_status, "Show system status and RXC estimate"},
    {"save", cmd_save, "Persist current configuration to flash"},
    {"wipe", cmd_wipe, "Erase persistent configuration from flash"},
    {"net", cmd_net, "Show current network status (W5500)"},
    {"set", cmd_set, "Set values (e.g. set gpio <pin> <0|1>, set net ip.local <addr>/<cidr>)"},
    {"get", cmd_get, "Get values (e.g. get gpio <pin>, get net ip.local)"},
    {"pininfo", cmd_pininfo, "Show pin details: pininfo <pin>"},
    {"reboot", cmd_reboot, "Reboot the device"},
};

static const category_t categories[] = {
    {"gpio", cat_gpio_set, cat_gpio_get, "GPIO controls and queries"},
    {"net", cat_net_set, cat_net_get, "Network configuration and queries"},
    {"v24", cat_v24_set, cat_v24_get, "V.24 config (inverted pins, baudrate)"},
};

static const subcmd_t net_subcmds[] = {
    {"ip.local", subcmd_set_ip_local, subcmd_get_ip_local, "Local IP address (CIDR)"},
    {"ip.remote", subcmd_set_ip_remote, subcmd_get_ip_remote, "Remote IP address"},
    {"ip.gateway", subcmd_set_ip_gateway, subcmd_get_ip_gateway, "Gateway IP address"},
    {"udp.port.local", subcmd_set_udp_port_local, subcmd_get_udp_port_local, "Local UDP port"},
    {"udp.port.remote", subcmd_set_udp_port_remote, subcmd_get_udp_port_remote, "Remote UDP port"},
};
#define INVERT_HELP \
    "Invert pins.\r\n \
    You can pass them as a comma seperated list like this:\r\n\
    \tset v24 invert txd,rxd\r\n\
    Each pin in the list is inverted, all others are non inverted."
static const subcmd_t v24_subcmds[] = {
    {"invert", subcmd_set_v24_inverted, subcmd_get_v24_inverted, INVERT_HELP},
    {"baudrate", subcmd_set_v24_baudrate, subcmd_get_v24_baudrate, "Baudrate"},
};

#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))
#define NUM_CATEGORIES ARRAY_LEN(categories)
#define NUM_NET_SUBCMDS ARRAY_LEN(net_subcmds)
#define NUM_V24_SUBCMDS ARRAY_LEN(v24_subcmds)
#define NUM_PINS get_num_pins()

static const V24_BAUDRATE_T v24_baudrates[] = {
    V24_BAUD_1200,  V24_BAUD_2400,  V24_BAUD_4800,  V24_BAUD_9600,   V24_BAUD_16000,
    V24_BAUD_19200, V24_BAUD_38400, V24_BAUD_57600, V24_BAUD_115200,
};
#define NUM_V24_BAUDRATES ARRAY_LEN(v24_baudrates)

static void dispatch_v24_polarities(const V24_POLARITIES_T* polarities)
{
    event_queue_data_t event_data = {.id = V24_POLARITIES};
    memcpy(&event_data.value.polarities, polarities, sizeof(V24_POLARITIES_T));

    event_t event = {
        .type      = EV_SET_V24_SETTINGS,
        .data_len  = sizeof(event_data),
        .is_inline = true,
    };
    memcpy(event.data.bytes, &event_data, sizeof(event_data));
    event_queue_push(&event);
}

static void dispatch_get_request(event_queue_data_types_t type, event_type_t event_type)
{
    event_queue_data_t get_request = {.id = type};

    event_t event = {
        .type      = event_type,
        .data_len  = sizeof(get_request),
        .is_inline = true,
    };
    memcpy(event.data.bytes, &get_request, sizeof(get_request));
    event_queue_push(&event);
}
static void subcmd_set_v24_inverted(const char* args)
{
    V24_POLARITIES_T polarities;
    if (parse_set_v24_polarities(args, &polarities) != E2S_OK)
    {
        printf("usage: set v24 polarities txd,rxd,rts\r\n");
        return;
    }
    dispatch_v24_polarities(&polarities);
}

static void subcmd_get_v24_inverted(const char* args)
{
    (void)args;
    dispatch_get_request(V24_POLARITIES, EV_GET_V24_SETTINGS);
}

static void dispatch_v24_baudrate(const V24_BAUDRATE_T* baudrate)
{
    event_queue_data_t event_data = {.id = V24_BAUDRATE};
    memcpy(&event_data.value.baudrate, baudrate, sizeof(V24_BAUDRATE_T));

    event_t event = {
        .type      = EV_SET_V24_SETTINGS,
        .data_len  = sizeof(event_data),
        .is_inline = true,
    };
    memcpy(event.data.bytes, &event_data, sizeof(event_data));
    event_queue_push(&event);
}

static void subcmd_set_v24_baudrate(const char* args)
{
    V24_BAUDRATE_T baudrate;
    if (parse_set_v24_baudrate(args, &baudrate) != E2S_OK)
    {
        printf("usage: set v24 baudrate 9600\r\n");
        return;
    }
    dispatch_v24_baudrate(&baudrate);
}

static void subcmd_get_v24_baudrate(const char* args)
{
    (void)args;
    dispatch_get_request(V24_BAUDRATE, EV_GET_V24_SETTINGS);
}

static void cat_v24_set(const char* args)
{
    LOG_DEBUG("set v24: args='%s'\r\n", args);
    if (args == NULL || args[0] == '\0')
    {
        printf("usage: set v24 <subcmd> <args>\r\n");
        printf("available v24 subcmds:\r\n");
        for (size_t i = 0; i < NUM_V24_SUBCMDS; i++)
        {
            printf("  %s  - %s\r\n", v24_subcmds[i].name, v24_subcmds[i].help);
        }
        return;
    }
    for (size_t i = 0; i < NUM_V24_SUBCMDS; i++)
    {
        size_t len = strlen(v24_subcmds[i].name);
        if (strncmp(args, v24_subcmds[i].name, len) == 0 && (args[len] == ' ' || args[len] == '\0'))
        {
            const char* sub_args = args[len] == ' ' ? args + len + 1 : "";
            v24_subcmds[i].set_handler(sub_args);
            return;
        }
    }
    printf("unknown v24 subcmd: '%s'\r\n", args);
}

static void cat_v24_get(const char* args)
{
    LOG_DEBUG("get v24: args='%s'\r\n", args);
    if (args == NULL || args[0] == '\0')
    {
        printf("usage: get v24 <subcmd>\r\n");
        printf("available v24 subcmds:\r\n");
        for (size_t i = 0; i < NUM_V24_SUBCMDS; i++)
        {
            printf("  %s  - %s\r\n", v24_subcmds[i].name, v24_subcmds[i].help);
        }
        return;
    }
    for (size_t i = 0; i < NUM_V24_SUBCMDS; i++)
    {
        size_t len = strlen(v24_subcmds[i].name);
        if (strncmp(args, v24_subcmds[i].name, len) == 0 && (args[len] == ' ' || args[len] == '\0'))
        {
            const char* sub_args = args[len] == ' ' ? args + len + 1 : "";
            v24_subcmds[i].get_handler(sub_args);
            return;
        }
    }
    printf("unknown v24 subcmd: '%s'\r\n", args);
}

static void cmd_reboot(const char* args)
{
    (void)args;
    printf("Rebooting...\r\n");
    watchdog_reboot(0, 0, 100); // small delay to let printf flush
}

static void dispatch_ip(const uint8_t* ip, const event_queue_data_types_t type)
{
    event_queue_data_t ip_event_data = {.id = type};
    memcpy(ip_event_data.value.ip, ip, 4); //

    event_t ip_event = {
        .type      = EV_SET_NET_SETTINGS,
        .data_len  = sizeof(ip_event_data),
        .is_inline = true,
    };
    memcpy(ip_event.data.bytes, &ip_event_data, sizeof(ip_event_data));
    event_queue_push(&ip_event);
}

static void subcmd_get_ip_local(const char* args)
{
    if (args != NULL && args[0] != '\0')
    {
        printf("usage: get net ip.local\r\n");
        return;
    }
    dispatch_get_request(NET_IP_LOCAL, EV_GET_NET_SETTINGS);
}

static void subcmd_set_ip_local(const char* args)
{
    uint8_t ip[4];
    uint8_t mask[4];
    if (parse_set_ip_args(args, ip, mask) != E2S_OK)
    {
        printf("usage: set net ip 192.168.29.2/24\r\n");
        return;
    }
    dispatch_ip(ip, NET_IP_LOCAL);
    dispatch_ip(mask, NET_IP_MASK);
}

static void subcmd_get_ip_remote(const char* args)
{
    if (args != NULL && args[0] != '\0')
    {
        printf("usage: get net ip.remote\r\n");
        return;
    }
    dispatch_get_request(NET_IP_REMOTE, EV_GET_NET_SETTINGS);
}

static void subcmd_set_ip_remote(const char* args)
{
    uint8_t ip[4];
    if (parse_set_ip_remote_args(args, ip) != E2S_OK)
    {
        printf("usage: set net ip.remote 192.168.29.2\r\n");
        return;
    }
    dispatch_ip(ip, NET_IP_REMOTE);
}

static void subcmd_get_ip_gateway(const char* args)
{
    if (args != NULL && args[0] != '\0')
    {
        printf("usage: get net ip.gateway\r\n");
        return;
    }
    dispatch_get_request(NET_IP_GATEWAY, EV_GET_NET_SETTINGS);
}

static void subcmd_set_ip_gateway(const char* args)
{
    uint8_t ip[4];
    if (parse_set_ip_remote_args(args, ip) != E2S_OK)
    {
        printf("usage: set net ip.gateway 192.168.29.1\r\n");
        return;
    }
    dispatch_ip(ip, NET_IP_GATEWAY);
}

static void dispatch_get_udp_port(const event_queue_data_types_t type)
{
    event_queue_data_t get_request = {.id = type};

    event_t event = {
        .type      = EV_GET_NET_SETTINGS,
        .data_len  = sizeof(get_request),
        .is_inline = true,
    };
    memcpy(event.data.bytes, &get_request, sizeof(get_request));
    event_queue_push(&event);
}

static void dispatch_set_udp_port(const event_queue_data_types_t type, uint16_t port)
{
    event_queue_data_t set_request = {.id = type, .value.port = port};

    event_t event = {
        .type      = EV_SET_NET_SETTINGS,
        .data_len  = sizeof(set_request),
        .is_inline = true,
    };
    memcpy(event.data.bytes, &set_request, sizeof(set_request));
    event_queue_push(&event);
}

static void subcmd_get_udp_port_local(const char* args)
{
    if (args != NULL && args[0] != '\0')
    {
        printf("usage: get net udp.port.local\r\n");
        return;
    }
    dispatch_get_udp_port(NET_PORT_LOCAL);
}

static void subcmd_set_udp_port_local(const char* args)
{
    uint16_t port = 0;
    if (parse_set_udp_port_local_args(args, &port) != E2S_OK)
    {
        printf("usage: set net udp.port.local 6969\r\n");
        return;
    }
    dispatch_set_udp_port(NET_PORT_LOCAL, port);
}

static void subcmd_get_udp_port_remote(const char* args)
{
    if (args != NULL && args[0] != '\0')
    {
        printf("usage: get net udp.port.remote\r\n");
        return;
    }

    dispatch_get_udp_port(NET_PORT_REMOTE);
}

static void subcmd_set_udp_port_remote(const char* args)
{
    uint16_t port = 0;
    if (parse_set_udp_port_remote_args(args, &port) != E2S_OK)
    {
        printf("usage: set net udp.port.remote 6969\r\n");
        return;
    }
    dispatch_set_udp_port(NET_PORT_REMOTE, port);
}

static void cat_net_get(const char* args)
{
    LOG_DEBUG("get net: args='%s'\r\n", args);
    if (args == NULL || args[0] == '\0')
    {
        printf("usage: get net <subcmd>\r\n");
        printf("available net subcmds:\r\n");
        for (size_t i = 0; i < NUM_NET_SUBCMDS; i++)
        {
            printf("  %s  - %s\r\n", net_subcmds[i].name, net_subcmds[i].help);
        }
        return;
    }
    for (size_t i = 0; i < NUM_NET_SUBCMDS; i++)
    {
        size_t len = strlen(net_subcmds[i].name);
        if (strncmp(args, net_subcmds[i].name, len) == 0 && (args[len] == ' ' || args[len] == '\0'))
        {
            const char* sub_args = args[len] == ' ' ? args + len + 1 : "";
            net_subcmds[i].get_handler(sub_args);
            return;
        }
    }
    printf("unknown net subcmd: '%s'\r\n", args);
}

static void cat_gpio_set(const char* args)
{
    char              pin_name[MAX_PIN_NAME_LEN];
    int               value;
    const pin_info_t* pin           = NULL;
    e2s_error_t       parser_result = parse_set_gpio_args(args, pin_name, &value, &pin);

    switch (parser_result)
    {
    case E2S_ERR_CLI_USAGE_SET:
        printf("usage:\r\n");
        printf("  set gpio <pin> <0|1>\r\n");
        return;
    case E2S_ERR_CLI_UNKNOWN_PIN:
        printf("unknown pin: '%s'\r\n", pin_name);
        return;
    case E2S_ERR_CLI_PIN_INPUT_ONLY:
        printf("pin '%s' is input-only\r\n", pin_name);
        return;
    case E2S_OK:
        break;
    default:
        // Unreachable
        return;
    }

    if (pin == NULL)
    {
        return;
    }

    gpio_init(pin->gpio_num);
    gpio_set_dir(pin->gpio_num, GPIO_OUT);
    gpio_put(pin->gpio_num, value);

    printf("set %s (pin %u) = %d\r\n", pin_name, pin->gpio_num, value);
}

static void cat_net_set(const char* args)
{
    if (args == NULL || args[0] == '\0')
    {
        printf("usage: set net <subcmd> <args>\r\n");
        printf("available net subcmds:\r\n");
        for (size_t i = 0; i < NUM_NET_SUBCMDS; i++)
        {
            printf("  %s  - %s\r\n", net_subcmds[i].name, net_subcmds[i].help);
        }
        return;
    }
    for (size_t i = 0; i < NUM_NET_SUBCMDS; i++)
    {
        size_t len = strlen(net_subcmds[i].name);
        if (strncmp(args, net_subcmds[i].name, len) == 0 && (args[len] == ' ' || args[len] == '\0'))
        {
            const char* sub_args = args[len] == ' ' ? args + len + 1 : "";
            net_subcmds[i].set_handler(sub_args);
            return;
        }
    }
    printf("unknown net subcmd: '%s'\r\n", args);
}

static void cat_gpio_get(const char* args)
{
    char              pin_name[MAX_PIN_NAME_LEN];
    const pin_info_t* pin = NULL;

    e2s_error_t parser_result = parse_get_args(args, pin_name, &pin);

    switch (parser_result)
    {
    case E2S_ERR_CLI_USAGE_GET:
        printf("usage: get <pin>\r\n");
        return;
    case E2S_ERR_CLI_UNKNOWN_PIN:
        printf("unknown pin: '%s'\r\n", pin_name);
        return;
    case E2S_OK:
        break;
    default:
        // Unreachable
        return;
    }

    if (pin == NULL)
    {
        return;
    }

    // Only initialize if not already a GPIO function
    if (gpio_get_function(pin->gpio_num) != GPIO_FUNC_SIO)
    {
        gpio_init(pin->gpio_num);
        gpio_set_dir(pin->gpio_num, GPIO_IN);
    }
    // If already GPIO but not configured as input/output, set as input
    else if (!gpio_is_dir_out(pin->gpio_num))
    {
        // Already initialized as GPIO input, no change needed
    }

    int  value     = gpio_get(pin->gpio_num);
    bool is_output = gpio_is_dir_out(pin->gpio_num);

    printf("get %s (pin %u) = %d [%s]\r\n", pin_name, pin->gpio_num, value,
           is_output ? "OUT" : "IN");
}

static void cmd_set(const char* args)
{
    if (args == NULL || args[0] == '\0')
    {
        printf("usage: set <category> <args>\r\n");
        printf("available categories:\r\n");
        for (size_t i = 0; i < NUM_CATEGORIES; i++)
        {
            printf("  %s  - %s\r\n", categories[i].name, categories[i].help);
        }
        return;
    }
    for (size_t i = 0; i < NUM_CATEGORIES; i++)
    {
        size_t len = strlen(categories[i].name);
        if (strncmp(args, categories[i].name, len) == 0 && args[len] == ' ')
        {
            categories[i].set_handler(args + len + 1);
            return;
        }
    }
    printf("unknown set category: '%s'\r\n", args);
}

static void cmd_get(const char* args)
{
    if (args == NULL || args[0] == '\0')
    {
        printf("usage: get <category> <args>\r\n");
        printf("available categories:\r\n");
        for (size_t i = 0; i < NUM_CATEGORIES; i++)
        {
            printf("  %s  - %s\r\n", categories[i].name, categories[i].help);
        }
        return;
    }
    for (size_t i = 0; i < NUM_CATEGORIES; i++)
    {
        size_t len = strlen(categories[i].name);
        if (strncmp(args, categories[i].name, len) == 0 && args[len] == ' ')
        {
            categories[i].get_handler(args + len + 1);
            return;
        }
    }
    printf("unknown get category: '%s'\r\n", args);
}

static void cmd_wipe(const char* args)
{
    (void)args;
    event_t wipe_event = {.type = EV_WIPE_CONFIG, .data = NULL, .data_len = 0};
    event_queue_push(&wipe_event);
}

static void cmd_save(const char* args)
{
    (void)args;
    event_t save_event = {.type = EV_SAVE_CONFIG, .data = NULL, .data_len = 0};
    event_queue_push(&save_event);
}

// Command handlers
static void cmd_help(const char* args)
{
    (void)args;

    size_t max_cmd = 0;
    for (size_t i = 0; i < NUM_COMMANDS; i++)
    {
        size_t len = strlen(commands[i].name);
        if (len > max_cmd)
        {
            max_cmd = len;
        }
    }

    printf("\r\nCommands:\r\n");
    for (size_t i = 0; i < NUM_COMMANDS; i++)
    {
        printf("  %-*s  %s\r\n", (int)max_cmd, commands[i].name, commands[i].help);
    }

    printf("\r\nSet/Get Categories:\r\n");
    for (size_t i = 0; i < NUM_CATEGORIES; i++)
    {
        printf("  %s  - %s\r\n", categories[i].name, categories[i].help);
    }

    printf("\r\nNet Subcommands:\r\n");
    for (size_t i = 0; i < NUM_NET_SUBCMDS; i++)
    {
        printf("  %s  - %s\r\n", net_subcmds[i].name, net_subcmds[i].help);
    }

    printf("\r\nV24 Subcommands:\r\n");
    for (size_t i = 0; i < NUM_V24_SUBCMDS; i++)
    {
        printf("  %s  - %s\r\n", v24_subcmds[i].name, v24_subcmds[i].help);
    }

    printf("\r\nPins:\r\n");
    const pin_info_t* pin_table = get_pin_table();
    for (size_t i = 0; i < NUM_PINS; i++)
    {
        printf("  %-10s  %s\r\n", pin_table[i].name, pin_table[i].is_output ? "OUT" : "IN");
    }

    printf("\r\nBaudrates:\r\n");
    printf("  ");
    for (size_t i = 0; i < NUM_V24_BAUDRATES; i++)
    {
        printf("%u", (unsigned)v24_baudrates[i]);
        if (i < NUM_V24_BAUDRATES - 1)
        {
            printf(", ");
        }
    }
    printf("\r\n");
}

static void cmd_status(const char* args)
{
    (void)args;
    printf("status: ok\r\n");
    printf("Current Baudrate estimation on pin %d: %.1f Hz\r\n", V24_RXC,
           baudrate_estimator_get_current_estimation(V24_RXC));
    event_t status_event = {.type = EV_STATUS, .data = NULL, .data_len = 0};
    event_queue_push(&status_event);
}

static void cmd_net(const char* args)
{
    if (args != NULL && args[0] != '\0')
    {
        printf("usage: net\r\n");
        return;
    }
    wiz_NetInfo net_info;
    wizchip_getnetinfo(&net_info);
    printf("ip=%u.%u.%u.%u gw=%u.%u.%u.%u\r\n", net_info.ip[0], net_info.ip[1], net_info.ip[2],
           net_info.ip[3], net_info.gw[0], net_info.gw[1], net_info.gw[2], net_info.gw[3]);
}

static void cmd_pininfo(const char* args)
{
    char pin_name[MAX_PIN_NAME_LEN];

    if (sscanf(args, "%15s", pin_name) != 1)
    {
        printf("usage: pininfo <pin>\r\n");
        return;
    }

    const pin_info_t* pin = find_pin(pin_name);
    if (!pin)
    {
        printf("unknown pin: '%s'\r\n", pin_name);
        return;
    }

    uint8_t gpio_num  = pin->gpio_num;
    bool    is_output = gpio_is_dir_out(gpio_num);
    int     value     = gpio_get(gpio_num);

    printf("Pin %s (GPIO %u):\r\n", pin_name, gpio_num);
    printf("  Direction: %s\r\n", is_output ? "OUTPUT" : "INPUT");
    printf("  Value: %d\r\n", value);
    printf("  Function: %d\r\n", gpio_get_function(gpio_num));
}

const char* get_command_name(int index)
{
    return commands[index].name;
}

void handle_cli_line(const char* line)
{
    char cmd[MAX_CMD_BUFFER_LEN];
    cmd[0] = '\0';
    char args[MAX_ARG_BUFFER_LEN];
    if (cli_parse(line, cmd, args) == E2S_OK)
    {
        // Look up command
        for (size_t i = 0; i < NUM_COMMANDS; i++)
        {
            if (strcmp(cmd, commands[i].name) == 0)
            {
                commands[i].handler(args);
                return;
            }
        }
        printf("unknown: '%s' (try 'help')\r\n", cmd);
    }
}
