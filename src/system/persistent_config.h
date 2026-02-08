
/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/persistent_config.h
 * Purpose: Persistent configuration data model and flash persistence API.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef PERSISTENT_CONFIG_H
#define PERSISTENT_CONFIG_H

// Related headers
// Standard library headers
#include <stddef.h>
#include <string.h>

// Library Headers
#include "hardware/flash.h"
#include "hardware/sync.h"

// Project Headers
#include "drivers/gpio_driver.h"
#include "drivers/pio_tx_rx_driver.h"
#include "drivers/w5500_driver.h"
#include "system/common.h"

// Generated headers

/**
 * @brief Persistent configuration blob stored in flash.
 */
typedef struct
{
    /** Magic marker to validate flash content. */
    uint32_t         magic;
    /** Schema version for migration support. */
    uint32_t         version;
    /** Local UDP endpoint. */
    UDP_CONFIG_T     local_config;
    /** Remote UDP endpoint. */
    UDP_CONFIG_T     remote_config;
    /** Network interface configuration. */
    NETWORK_CONFIG_T net_config;
    /** V.24 line/baud configuration. */
    V24_CONFIG_T     v24_config;
    /** Default runtime log level. */
    log_level_t      log_level;
} config_t;

/**
 * @brief Read configuration from flash.
 * @param cfg Destination object.
 * @return true when valid config was read.
 */
bool config_read(config_t* cfg);

/**
 * @brief Write configuration to flash.
 * @param cfg Source config object.
 */
void config_write(const config_t* cfg);

/**
 * @brief Check if flash configuration magic marker is valid.
 * @return true when stored configuration is valid.
 */
bool config_is_valid(void);

/**
 * @brief Print current configuration to console/log output.
 */
void dump_config(void);

/**
 * @brief Erase persistent configuration sector.
 */
void config_wipe(void);

/**
 * @brief Print RAM usage statistics.
 */
void print_memory_usage(void);

/**
 * @brief Print flash usage statistics.
 */
void print_flash_usage(void);

#endif /* PERSISTENT_CONFIG_H */
