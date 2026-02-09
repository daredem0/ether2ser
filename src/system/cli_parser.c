/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/cli_parser.c
 * Purpose: CLI line parsing and pin lookup.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "cli_parser.h"

// Standard library headers
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Project Headers
#include "drivers/v24_config.h"
#include "platform/pinmap.h"
#include "system/error.h"

// Generated headers
// Todo: Get these entries from main
static const pin_info_t pin_table[] = {
    {"txd", V24_TXD, true},       {"rxd", V24_RXD, false},
    {"rts", V24_RTS, true},       {"cts", V24_CTS, false},
    {"dtr", V24_DTR, true},       {"dsr", V24_DSR, false},
    {"dcd", V24_DCD, false},      {"tx_active", V24_TX_ACTIVE, true},
    {"led", V24_STATUS_LED, true}};

#define NUM_PINS (sizeof(pin_table) / sizeof(pin_table[0]))

size_t get_num_pins(void)
{
    return NUM_PINS;
}

// cli_parser.c
static bool prefix_to_mask(uint8_t prefix, uint8_t mask[4])
{
    if (prefix > 32)
    {
        return false;
    }
    uint32_t m = prefix == 0 ? 0 : 0xFFFFFFFFu << (32 - prefix);
    mask[0]    = (m >> 24) & 0xFF;
    mask[1]    = (m >> 16) & 0xFF;
    mask[2]    = (m >> 8) & 0xFF;
    mask[3]    = (m >> 0) & 0xFF;
    return true;
}

static bool parse_u32_strict(const char* s, uint32_t* out)
{
    if (s == NULL || out == NULL || *s == '\0')
    {
        return false;
    }

    errno     = 0;
    char* end = NULL;
    unsigned long v = strtoul(s, &end, 10);
    if (errno == ERANGE || end == s || *end != '\0' || v > UINT32_MAX)
    {
        return false;
    }

    *out = (uint32_t)v;
    return true;
}

static bool parse_i32_strict(const char* s, int32_t* out)
{
    if (s == NULL || out == NULL || *s == '\0')
    {
        return false;
    }

    errno     = 0;
    char* end = NULL;
    long v    = strtol(s, &end, 10);
    if (errno == ERANGE || end == s || *end != '\0' || v < INT32_MIN || v > INT32_MAX)
    {
        return false;
    }

    *out = (int32_t)v;
    return true;
}

static bool parse_ipv4_strict(const char* s, uint8_t ip[4])
{
    if (s == NULL || ip == NULL)
    {
        return false;
    }

    const char* p = s;
    for (size_t i = 0; i < 4; i++)
    {
        if (*p == '\0')
        {
            return false;
        }

        errno     = 0;
        char* end = NULL;
        unsigned long octet = strtoul(p, &end, 10);
        if (errno == ERANGE || end == p || octet > UINT8_MAX)
        {
            return false;
        }

        if (i < 3)
        {
            if (*end != '.')
            {
                return false;
            }
            p = end + 1;
        }
        else if (*end != '\0')
        {
            return false;
        }

        ip[i] = (uint8_t)octet;
    }

    return true;
}

static e2s_error_t parse_ipv4_with_optional_prefix(const char* args, const char* prefix,
                                                   uint8_t ip[4])
{
    const char* s = args;
    if (prefix != NULL)
    {
        size_t len = strlen(prefix);
        if (strncmp(args, prefix, len) == 0 && args[len] == ' ')
        {
            s = args + len + 1;
        }
    }

    if (!parse_ipv4_strict(s, ip))
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    return E2S_OK;
}

static e2s_error_t parse_u16_with_optional_prefix(const char* args, const char* prefix,
                                                  uint16_t* value)
{
    const char* s = args;
    if (prefix != NULL)
    {
        size_t len = strlen(prefix);
        if (strncmp(args, prefix, len) == 0 && args[len] == ' ')
        {
            s = args + len + 1;
        }
    }

    uint32_t v = 0;
    if (!parse_u32_strict(s, &v) || v > UINT16_MAX)
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    *value = (uint16_t)v;
    return E2S_OK;
}

e2s_error_t parse_set_ip_args(const char* args, uint8_t ip[4], uint8_t mask[4])
{
    if (args == NULL || ip == NULL || mask == NULL)
    {
        return E2S_ERR_CLI_USAGE_SET;
    }

    const char* s = args;
    if (strncmp(s, "ip ", 3) == 0)
    {
        s += 3;
    }

    char buf[32];
    size_t len = strnlen(s, sizeof(buf));
    if (len == sizeof(buf))
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    memcpy(buf, s, len + 1);

    char* slash = strchr(buf, '/');
    if (slash == NULL)
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    *slash = '\0';

    uint32_t prefix = 0;
    if (!parse_u32_strict(slash + 1, &prefix))
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    if (!parse_ipv4_strict(buf, ip))
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    if (!prefix_to_mask((uint8_t)prefix, mask))
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    return E2S_OK;
}

e2s_error_t parse_set_gpio_args(const char* args, char* pin_name, int* value,
                                const pin_info_t** pin)
{
    if (args == NULL || pin_name == NULL || value == NULL || pin == NULL)
    {
        return E2S_ERR_CLI_USAGE_SET;
    }

    char buf[64];
    size_t len = strnlen(args, sizeof(buf));
    if (len == sizeof(buf))
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    memcpy(buf, args, len + 1);

    char* saveptr = NULL;
    char* tok_pin = strtok_r(buf, " ", &saveptr);
    char* tok_val = strtok_r(NULL, " ", &saveptr);
    char* tok_extra = strtok_r(NULL, " ", &saveptr);
    if (tok_pin == NULL || tok_val == NULL || tok_extra != NULL)
    {
        return E2S_ERR_CLI_USAGE_SET;
    }

    size_t pin_len = strnlen(tok_pin, 16);
    if (pin_len == 16)
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    memcpy(pin_name, tok_pin, pin_len + 1);

    int32_t parsed_value = 0;
    if (!parse_i32_strict(tok_val, &parsed_value) || (parsed_value != 0 && parsed_value != 1))
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    *value = (int)parsed_value;

    *pin = find_pin(pin_name);
    if (!*pin)
    {
        return E2S_ERR_CLI_UNKNOWN_PIN;
    }

    if (!(*pin)->is_output)
    {
        return E2S_ERR_CLI_PIN_INPUT_ONLY;
    }
    return E2S_OK;
}

e2s_error_t parse_set_net_ip_args(const char* args, uint8_t ip[4], uint8_t mask[4])
{
    if (strncmp(args, "net ", 4) != 0)
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    return parse_set_ip_args(args + 4, ip, mask);
}

e2s_error_t parse_set_ip_remote_args(const char* args, uint8_t ip[4])
{
    return parse_ipv4_with_optional_prefix(args, "ip", ip);
}

e2s_error_t parse_set_gateway_args(const char* args, uint8_t ip[4])
{
    return parse_ipv4_with_optional_prefix(args, "gateway", ip);
}

e2s_error_t parse_set_udp_port_local_args(const char* args, uint16_t* port)
{
    return parse_u16_with_optional_prefix(args, "port", port);
}

e2s_error_t parse_set_udp_port_remote_args(const char* args, uint16_t* port)
{
    return parse_u16_with_optional_prefix(args, "port", port);
}

e2s_error_t parse_set_v24_polarities(const char* args, V24_POLARITIES_T* polarities)
{
    if (polarities == NULL)
    {
        return E2S_ERR_CLI_USAGE_SET;
    }

    memset(polarities, 0, sizeof(*polarities));

    if (args == NULL)
    {
        return E2S_OK;
    }

    while (*args == ' ')
    {
        args++;
    }
    if (*args == '\0')
    {
        return E2S_OK;
    }

    const char* s = args;
    if (strncmp(s, "invert ", 7) == 0)
    {
        s += 7;
    }

    char   buf[64];
    size_t len = strnlen(s, sizeof(buf));
    if (len == sizeof(buf))
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    memcpy(buf, s, len + 1);

    char* token = strtok(buf, ",");
    while (token != NULL)
    {
        while (*token == ' ')
        {
            token++;
        }
        char* end = token + strlen(token);
        while (end > token && end[-1] == ' ')
        {
            end--;
        }
        *end = '\0';

        if (*token == '\0')
        {
            return E2S_ERR_CLI_USAGE_SET;
        }

        const pin_info_t* pin = find_pin(token);
        if (pin == NULL)
        {
            return E2S_ERR_CLI_USAGE_SET;
        }

        if (strcmp(token, "txd") == 0)
        {
            polarities->tx_polarities.txd_inverted = true;
        }
        else if (strcmp(token, "rts") == 0)
        {
            polarities->tx_polarities.rts_inverted = true;
        }
        else if (strcmp(token, "cts") == 0)
        {
            polarities->tx_polarities.cts_inverted = true;
        }
        else if (strcmp(token, "dtr") == 0)
        {
            polarities->tx_polarities.dtr_inverted = true;
        }
        else if (strcmp(token, "rxd") == 0)
        {
            polarities->rx_polarities.rxd_inverted = true;
        }
        else if (strcmp(token, "dcd") == 0)
        {
            polarities->rx_polarities.dcd_inverted = true;
        }
        else
        {
            return E2S_ERR_CLI_USAGE_SET;
        }

        token = strtok(NULL, ",");
    }

    return E2S_OK;
}

e2s_error_t parse_set_v24_baudrate(const char* args, V24_BAUDRATE_T* baudrate)
{
    if (baudrate == NULL)
    {
        return E2S_ERR_CLI_USAGE_SET;
    }

    const char* s = args;
    if (s == NULL)
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    while (*s == ' ')
    {
        s++;
    }
    if (*s == '\0')
    {
        return E2S_ERR_CLI_USAGE_SET;
    }

    uint32_t v = 0;
    if (!parse_u32_strict(s, &v))
    {
        return E2S_ERR_CLI_USAGE_SET;
    }

    switch (v)
    {
    case V24_BAUD_1200:
    case V24_BAUD_2400:
    case V24_BAUD_4800:
    case V24_BAUD_9600:
    case V24_BAUD_16000:
    case V24_BAUD_19200:
    case V24_BAUD_38400:
    case V24_BAUD_57600:
    case V24_BAUD_115200:
        *baudrate = (V24_BAUDRATE_T)v;
        return E2S_OK;
    default:
        return E2S_ERR_CLI_USAGE_SET;
    }
}

const pin_info_t* get_pin_table(void)
{
    return pin_table;
}

// Helper function to find pin by name
const pin_info_t* find_pin(const char* name)
{
    for (size_t i = 0; i < NUM_PINS; i++)
    {
        if (strcmp(name, pin_table[i].name) == 0)
        {
            return &pin_table[i];
        }
    }
    return NULL;
}

e2s_error_t parse_get_args(const char* args, char* pin_name, const pin_info_t** pin)
{

    if (sscanf(args, "%15s", pin_name) != 1)
    {
        return E2S_ERR_CLI_USAGE_GET;
    }

    *pin = find_pin(pin_name);
    if (!*pin)
    {
        return E2S_ERR_CLI_UNKNOWN_PIN;
    }
    return E2S_OK;
}

e2s_error_t cli_parse(const char* line, char* cmd, size_t cmd_cap, char* args, size_t args_cap)

{
    if (!line || !cmd || !args || cmd_cap == 0 || args_cap == 0)
    {
        return E2S_ERR_CLI_USAGE_SET;
    }

    if (line[0] == '\0')
    {
        cmd[0]  = '\0';
        args[0] = '\0';
        return E2S_ERR_CLI_EMPTY_LINE;
    }

    // Skip leading spaces
    while (*line == ' ')
    {
        line++;
    }
    if (*line == '\0')
    {
        cmd[0]  = '\0';
        args[0] = '\0';
        return E2S_ERR_CLI_EMPTY_LINE;
    }

    // cmd = first token
    const char* cmd_end = line;
    while (*cmd_end && *cmd_end != ' ')
    {
        cmd_end++;
    }

    size_t cmd_len = (size_t)(cmd_end - line);
    if (cmd_len + 1 > cmd_cap)
    {
        return E2S_ERR_CLI_LINE_TRUNCATED;
    }

    memcpy(cmd, line, cmd_len);
    cmd[cmd_len] = '\0';

    // args = remainder (trim leading spaces)
    const char* arg_start = cmd_end;
    while (*arg_start == ' ')
    {
        arg_start++;
    }

    int written = snprintf(args, args_cap, "%s", arg_start);
    if (written < 0)
    {
        return E2S_ERR_CLI_LINE_FORMAT;
    }
    if ((size_t)written >= args_cap)
    {
        return E2S_ERR_CLI_LINE_TRUNCATED;
    }

    return E2S_OK;
}
