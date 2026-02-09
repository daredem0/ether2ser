/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/cli_parser.h
 * Purpose: CLI parsing API and pin metadata types.
 *
 * SPDX-License-Identifier: Apache-2.0
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
#include "drivers/gpio_driver.h"
#include "drivers/v24_config.h"
#include "system/error.h"

// Generated headers

/**
 * @brief Pin metadata entry used by CLI lookup and validation.
 */
typedef struct
{
    /** Human-readable CLI pin name. */
    const char* name;
    /** GPIO number. */
    uint8_t gpio_num;
    /** True if pin is output-capable in CLI set operation. */
    bool is_output;
} pin_info_t;

/**
 * @brief Return number of entries in the CLI pin table.
 */
size_t get_num_pins(void);

/**
 * @brief Split one CLI line into command and argument string.
 * @param line Input command line.
 * @param cmd Output command token buffer.
 * @param cmd_cap Command token buffer capacity.
 * @param args Output argument string buffer.
 * @param args_cap Args token buffer capacity.
 * @return Parse status.
 */
e2s_error_t cli_parse(const char* line, char* cmd, size_t cmd_cap, char* args, size_t args_cap);
/**
 * @brief Parse `get` command arguments and resolve pin metadata.
 * @param args Argument string.
 * @param pin_name Output pin name.
 * @param pin Resolved pin metadata entry.
 * @return Parse status.
 */
e2s_error_t parse_get_args(const char* args, char* pin_name, const pin_info_t** pin);

/**
 * @brief Lookup pin metadata by name.
 * @param name Pin name.
 * @return Pointer to pin metadata or `NULL`.
 */
const pin_info_t* find_pin(const char* name);

/**
 * @brief Return pointer to static pin metadata table.
 */
const pin_info_t* get_pin_table(void);

/**
 * @brief Parse ip_addr and optional netmask values.
 * @param args Argument string.
 * @param ip_addr Output ip_addrv4 address.
 * @param netmask Output subnet netmas.
 * @return Parse status.
 */
e2s_error_t parse_set_ip_args(const char* args, uint8_t ip_addr[4], uint8_t netmask[4]);

/**
 * @brief Parse GPIO set command arguments.
 * @param args Argument string.
 * @param pin_name Output pin name.
 * @param value Output pin value.
 * @param pin Resolved pin metadata entry.
 * @return Parse status.
 */
e2s_error_t parse_set_gpio_args(const char* args, char* pin_name, int* value,
                                const pin_info_t** pin);

/**
 * @brief Parse network local ip_addr/subnet arguments.
 * @param args Argument string.
 * @param ip_addr Output ip_addrv4 address.
 * @param netmask Output subnet netmask.
 * @return Parse status.
 */
e2s_error_t parse_set_net_ip_args(const char* args, uint8_t ip_addr[4], uint8_t netmask[4]);

/**
 * @brief Parse remote ip_addr argument.
 * @param args Argument string.
 * @param ip_addr Output ip_addrv4 address.
 * @return Parse status.
 */
e2s_error_t parse_set_ip_remote_args(const char* args, uint8_t ip_addr[4]);

/**
 * @brief Parse gateway ip_addr argument.
 * @param args Argument string.
 * @param ip_addr Output ip_addrv4 address.
 * @return Parse status.
 */
e2s_error_t parse_set_gateway_args(const char* args, uint8_t ip_addr[4]);

/**
 * @brief Parse local UDP port argument.
 * @param args Argument string.
 * @param port Output UDP port.
 * @return Parse status.
 */
e2s_error_t parse_set_udp_port_local_args(const char* args, uint16_t* port);

/**
 * @brief Parse remote UDP port argument.
 * @param args Argument string.
 * @param port Output UDP port.
 * @return Parse status.
 */
e2s_error_t parse_set_udp_port_remote_args(const char* args, uint16_t* port);

/**
 * @brief Parse V.24 polarities argument list.
 * @param args Argument string.
 * @param polarities Output polarity configuration.
 * @return Parse status.
 */
e2s_error_t parse_set_v24_polarities(const char* args, V24_POLARITIES_T* polarities);

/**
 * @brief Parse V.24 baudrate argument.
 * @param args Argument string.
 * @param baudrate Output baudrate value.
 * @return Parse status.
 */
e2s_error_t parse_set_v24_baudrate(const char* args, V24_BAUDRATE_T* baudrate);

#endif /* CLI_PARSER_H */
