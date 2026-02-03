/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/cli_parser.h
 * Purpose: CLI parsing API and pin metadata types.
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef CLI_PARSER_H
#define CLI_PARSER_H

// Related headers

// Standard library headers
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Project Headers
#include "error.h"

// Generated headers

// Pin lookup table
typedef struct
{
    const char* name;
    uint8_t     gpio_num;
    bool        is_output;
} pin_info_t;

size_t            get_num_pins(void);
e2s_error_t       cli_parse(const char* line, char* cmd, char* args);
e2s_error_t       parse_get_args(const char* args, char* pin_name, const pin_info_t** pin);
const pin_info_t* find_pin(const char* name);
const pin_info_t* get_pin_table(void);
e2s_error_t       parse_set_ip_args(const char* args, uint8_t ip[4], uint8_t mask[4]);
e2s_error_t       parse_set_gpio_args(const char* args, char* pin_name, int* value,
                                      const pin_info_t** pin);
e2s_error_t       parse_set_net_ip_args(const char* args, uint8_t ip[4], uint8_t mask[4]);

#endif /* CLI_PARSER_H */
