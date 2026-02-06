

/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/drivers/pio_tx_rx_driver.h
 * Purpose: PIO TX clock driver API and baudrate definitions.
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef PIO_TX_RX_DRIVER_H
#define PIO_TX_RX_DRIVER_H
// Related headers

// Standard library headers
#include <stdbool.h>
#include <stdint.h>

// Library Headers
#include "hardware/pio.h"

// Project Headers
#include "drivers/gpio_driver.h"
#include "drivers/v24_config.h"

// Generated headers

void tx_clock_init(PIO pio, uint pio_sm, V24_BAUDRATE_T baudrate, V24_TX_POLARITIES_T* polarities);
bool tx_poll(void);
bool tx_put(uint8_t data);

void rx_clock_init(PIO pio, uint pio_sm, V24_RX_POLARITIES_T* polarities);
bool rx_get(uint8_t* data);

void init_v24_config(V24_CONFIG_T* config, V24_BAUDRATE_T baudrate);

#endif /* PIO_TX_RX_DRIVER_H */
