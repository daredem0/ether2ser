/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/cli_parser.c
 * Purpose: CLI line parsing and pin lookup.
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "cli_parser.h"

// Standard library headers
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Project Headers
#include "error.h"
#include "platform/pinmap.h"

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

    unsigned a, b, c, d;
    if (sscanf(s, "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    if (a > 255 || b > 255 || c > 255 || d > 255)
    {
        return E2S_ERR_CLI_USAGE_SET;
    }

    ip[0] = a;
    ip[1] = b;
    ip[2] = c;
    ip[3] = d;
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

    unsigned v;
    if (sscanf(s, "%u", &v) != 1 || v > 0xFFFFu)
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    *value = (uint16_t)v;
    return E2S_OK;
}

e2s_error_t parse_set_ip_args(const char* args, uint8_t ip[4], uint8_t mask[4])
{
    unsigned a, b, c, d, prefix;
    if (sscanf(args, "%u.%u.%u.%u/%u", &a, &b, &c, &d, &prefix) != 5 &&
        sscanf(args, "ip %u.%u.%u.%u/%u", &a, &b, &c, &d, &prefix) != 5)
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    if (a > 255 || b > 255 || c > 255 || d > 255)
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    if (!prefix_to_mask((uint8_t)prefix, mask))
    {
        return E2S_ERR_CLI_USAGE_SET;
    }

    ip[0] = a;
    ip[1] = b;
    ip[2] = c;
    ip[3] = d;
    return E2S_OK;
}

e2s_error_t parse_set_gpio_args(const char* args, char* pin_name, int* value,
                                const pin_info_t** pin)
{
    if (sscanf(args, "%15s %d", pin_name, value) != 2 || (*value != 0 && *value != 1))
    {
        return E2S_ERR_CLI_USAGE_SET;
    }

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

e2s_error_t cli_parse(const char* line, char* cmd, char* args)
{

    if (line[0] == '\0')
    {
        return E2S_ERR_CLI_EMPTY_LINE;
    }

    // Parse command and arguments
    int n = sscanf(line, "%15s", cmd);
    if (n == 1)
    {
        // Find start of arguments
        args[0]                 = '\0';
        const char* first_space = strchr(line, ' ');
        if (first_space)
        {
            const char* arg_start = first_space + 1;
            while (*arg_start == ' ')
            {
                arg_start++;
            }
            strcpy(args, arg_start);
        }
    }
    return E2S_OK;
}
