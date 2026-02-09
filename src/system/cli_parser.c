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
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Project Headers
#include "drivers/v24_config.h"
#include "platform/pinmap.h"
#include "system/common.h"
#include "system/error.h"

// Generated headers
// Todo: Get these entries from main
static const pin_info_t pin_table[] = {
    {"txd", V24_TXD, true},       {"rxd", V24_RXD, false},
    {"rts", V24_RTS, true},       {"cts", V24_CTS, false},
    {"dtr", V24_DTR, true},       {"dsr", V24_DSR, false},
    {"dcd", V24_DCD, false},      {"tx_active", V24_TX_ACTIVE, true},
    {"led", V24_STATUS_LED, true}};

#define NUM_PINS ARRAY_LEN(pin_table)
#define DECIMAL_BASE 10U

size_t get_num_pins(void)
{
    return NUM_PINS;
}

// cli_parser.c
static bool prefix_to_mask(uint8_t prefix, uint8_t netmask[4])
{
    if (prefix > 32)
    {
        return false;
    }
    uint32_t mask_word = prefix == 0 ? 0 : UINT32_ALL_ONES << (32 - prefix);
    netmask[0]         = (mask_word >> 24) & 0xFF;
    netmask[1]         = (mask_word >> 16) & 0xFF;
    netmask[2]         = (mask_word >> 8) & 0xFF;
    netmask[3]         = (mask_word >> 0) & 0xFF;
    return true;
}

static bool parse_u32_strict(const char* input_str, uint32_t* output_value)
{
    if (input_str == NULL || output_value == NULL || *input_str == '\0')
    {
        return false;
    }

    errno                      = 0;
    char*         end_ptr      = NULL;
    unsigned long parsed_value = strtoul(input_str, &end_ptr, DECIMAL_BASE);
    if (errno == ERANGE || end_ptr == input_str || *end_ptr != '\0' || parsed_value > UINT32_MAX)
    {
        return false;
    }

    *output_value = (uint32_t)parsed_value;
    return true;
}

static bool parse_i32_strict(const char* input_str, int32_t* output_value)
{
    if (input_str == NULL || output_value == NULL || *input_str == '\0')
    {
        return false;
    }

    errno              = 0;
    char* end_ptr      = NULL;
    long  parsed_value = strtol(input_str, &end_ptr, DECIMAL_BASE);
    if (errno == ERANGE || end_ptr == input_str || *end_ptr != '\0' || parsed_value < INT32_MIN ||
        parsed_value > INT32_MAX)
    {
        return false;
    }

    *output_value = (int32_t)parsed_value;
    return true;
}

static bool parse_ipv4_strict(const char* input_str, uint8_t ip_addr[4])
{
    if (input_str == NULL || ip_addr == NULL)
    {
        return false;
    }

    const char* cursor = input_str;
    for (size_t octet_index = 0; octet_index < 4; octet_index++)
    {
        if (*cursor == '\0')
        {
            return false;
        }

        errno                     = 0;
        char*         end_ptr     = NULL;
        unsigned long octet_value = strtoul(cursor, &end_ptr, DECIMAL_BASE);
        if (errno == ERANGE || end_ptr == cursor || octet_value > UINT8_MAX)
        {
            return false;
        }

        if (octet_index < 3)
        {
            if (*end_ptr != '.')
            {
                return false;
            }
            cursor = end_ptr + 1;
        }
        else if (*end_ptr != '\0')
        {
            return false;
        }

        ip_addr[octet_index] = (uint8_t)octet_value;
    }

    return true;
}

static e2s_error_t parse_ipv4_with_optional_prefix(const char* args, const char* prefix,
                                                   uint8_t ip_addr[4])
{
    const char* input_str = args;
    if (prefix != NULL)
    {
        size_t prefix_len = strlen(prefix);
        if (strncmp(args, prefix, prefix_len) == 0 && args[prefix_len] == ' ')
        {
            input_str = args + prefix_len + 1;
        }
    }

    if (!parse_ipv4_strict(input_str, ip_addr))
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    return E2S_OK;
}

static e2s_error_t parse_u16_with_optional_prefix(const char* args, const char* prefix,
                                                  uint16_t* value)
{
    const char* input_str = args;
    if (prefix != NULL)
    {
        size_t prefix_len = strlen(prefix);
        if (strncmp(args, prefix, prefix_len) == 0 && args[prefix_len] == ' ')
        {
            input_str = args + prefix_len + 1;
        }
    }

    uint32_t parsed_value = 0;
    if (!parse_u32_strict(input_str, &parsed_value) || parsed_value > UINT16_MAX)
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    *value = (uint16_t)parsed_value;
    return E2S_OK;
}

e2s_error_t parse_set_ip_args(const char* args, uint8_t ip_addr[4], uint8_t netmask[4])
{
    if (args == NULL || ip_addr == NULL || netmask == NULL)
    {
        return E2S_ERR_CLI_USAGE_SET;
    }

    const char* input_str = args;
    if (strncmp(input_str, "ip ", 3) == 0)
    {
        input_str += 3;
    }

    char   address_buf[32];
    size_t input_len = strnlen(input_str, sizeof(address_buf));
    if (input_len == sizeof(address_buf))
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    memcpy(address_buf, input_str, input_len + 1);

    char* slash_pos = strchr(address_buf, '/');
    if (slash_pos == NULL)
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    *slash_pos = '\0';

    uint32_t prefix = 0;
    if (!parse_u32_strict(slash_pos + 1, &prefix))
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    if (!parse_ipv4_strict(address_buf, ip_addr))
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    if (!prefix_to_mask((uint8_t)prefix, netmask))
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

    char   buf[64];
    size_t len = strnlen(args, sizeof(buf));
    if (len == sizeof(buf))
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    memcpy(buf, args, len + 1);

    char*       saveptr   = NULL;
    char*       tok_pin   = strtok_r(buf, " ", &saveptr);
    const char* tok_val   = strtok_r(NULL, " ", &saveptr);
    const char* tok_extra = strtok_r(NULL, " ", &saveptr);
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

e2s_error_t parse_set_net_ip_args(const char* args, uint8_t ip_addr[4], uint8_t netmask[4])
{
    if (strncmp(args, "net ", 4) != 0)
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    return parse_set_ip_args(args + 4, ip_addr, netmask);
}

e2s_error_t parse_set_ip_remote_args(const char* args, uint8_t ip_addr[4])
{
    return parse_ipv4_with_optional_prefix(args, "ip", ip_addr);
}

e2s_error_t parse_set_gateway_args(const char* args, uint8_t ip_addr[4])
{
    return parse_ipv4_with_optional_prefix(args, "gateway", ip_addr);
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

    const char* token_list = args;
    if (strncmp(token_list, "invert ", (CHAR_BIT - 1U)) == 0)
    {
        token_list += (CHAR_BIT - 1U);
    }

    char   token_buf[64];
    size_t token_len = strnlen(token_list, sizeof(token_buf));
    if (token_len == sizeof(token_buf))
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    memcpy(token_buf, token_list, token_len + 1);

    char* token_ptr = strtok(token_buf, ",");
    while (token_ptr != NULL)
    {
        while (*token_ptr == ' ')
        {
            token_ptr++;
        }
        char* token_end = token_ptr + strlen(token_ptr);
        while (token_end > token_ptr && token_end[-1] == ' ')
        {
            token_end--;
        }
        *token_end = '\0';

        if (*token_ptr == '\0')
        {
            return E2S_ERR_CLI_USAGE_SET;
        }

        const pin_info_t* pin_info = find_pin(token_ptr);
        if (pin_info == NULL)
        {
            return E2S_ERR_CLI_USAGE_SET;
        }

        if (strcmp(token_ptr, "txd") == 0)
        {
            polarities->tx_polarities.txd_inverted = true;
        }
        else if (strcmp(token_ptr, "rts") == 0)
        {
            polarities->tx_polarities.rts_inverted = true;
        }
        else if (strcmp(token_ptr, "cts") == 0)
        {
            polarities->tx_polarities.cts_inverted = true;
        }
        else if (strcmp(token_ptr, "dtr") == 0)
        {
            polarities->tx_polarities.dtr_inverted = true;
        }
        else if (strcmp(token_ptr, "rxd") == 0)
        {
            polarities->rx_polarities.rxd_inverted = true;
        }
        else if (strcmp(token_ptr, "dcd") == 0)
        {
            polarities->rx_polarities.dcd_inverted = true;
        }
        else
        {
            return E2S_ERR_CLI_USAGE_SET;
        }

        token_ptr = strtok(NULL, ",");
    }

    return E2S_OK;
}

e2s_error_t parse_set_v24_baudrate(const char* args, V24_BAUDRATE_T* baudrate)
{
    if (baudrate == NULL)
    {
        return E2S_ERR_CLI_USAGE_SET;
    }

    const char* input_str = args;
    if (input_str == NULL)
    {
        return E2S_ERR_CLI_USAGE_SET;
    }
    while (*input_str == ' ')
    {
        input_str++;
    }
    if (*input_str == '\0')
    {
        return E2S_ERR_CLI_USAGE_SET;
    }

    uint32_t baud_value = 0;
    if (!parse_u32_strict(input_str, &baud_value))
    {
        return E2S_ERR_CLI_USAGE_SET;
    }

    switch (baud_value)
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
        *baudrate = (V24_BAUDRATE_T)baud_value;
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
    for (size_t pin_index = 0; pin_index < NUM_PINS; pin_index++)
    {
        if (strcmp(name, pin_table[pin_index].name) == 0)
        {
            return &pin_table[pin_index];
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
