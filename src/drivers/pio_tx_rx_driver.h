

/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/drivers/pio_tx_rx_driver.h
 * Purpose: PIO TX clock driver API and baudrate definitions.
 *
 * SPDX-License-Identifier: Apache-2.0
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

/**
 * @brief Initialize TX clock/data PIO state machine.
 * @param pio PIO instance.
 * @param pio_sm State machine index.
 * @param baudrate Target synchronous baudrate.
 * @param polarities TX signal polarity configuration.
 */
void tx_clock_init(PIO pio, uint pio_sm, V24_BAUDRATE_T baudrate, V24_TX_POLARITIES_T* polarities);

/**
 * @brief Poll TX completion/holdoff state and manage RTS release.
 * @return true if TX path is idle after polling, false otherwise.
 */
bool tx_poll(void);

/**
 * @brief Queue one byte to the TX PIO FIFO.
 * @param data Byte to transmit.
 * @return true if accepted, false if FIFO/full-state prevents enqueue.
 */
bool tx_put(uint8_t data);

/**
 * @brief Initialize RX clock/data PIO state machine.
 * @param pio PIO instance.
 * @param pio_sm State machine index.
 * @param polarities RX signal polarity configuration.
 */
void rx_clock_init(PIO pio, uint pio_sm, V24_RX_POLARITIES_T* polarities);

/**
 * @brief Read one received byte from RX PIO FIFO.
 * @param data Destination pointer for the received byte.
 * @return true if a byte was read, false if RX FIFO is empty.
 */
bool rx_get(uint8_t* data);

/**
 * @brief Initialize V.24 runtime configuration structure.
 * @param config Destination configuration object.
 * @param baudrate Initial baudrate.
 */
void init_v24_config(V24_CONFIG_T* config, V24_BAUDRATE_T baudrate);

/**
 * @brief Reinitialize V.24 runtime configuration and derived timing values.
 * @param config Destination configuration object.
 * @param baudrate New baudrate.
 */
void reinit_v24_config(V24_CONFIG_T* config, V24_BAUDRATE_T baudrate);

/**
 * @brief Initialize optional LED activity mirror PIO program.
 */
void led_mirror_init(void);

/**
 * @brief Apply TX runtime settings to an already configured TX PIO SM.
 * @param pio PIO instance.
 * @param pio_sm State machine index.
 * @param baudrate New baudrate.
 * @param polarities TX polarity settings.
 */
void tx_clock_update_settings(PIO pio, uint pio_sm, V24_BAUDRATE_T baudrate,
                              V24_TX_POLARITIES_T* polarities);

/**
 * @brief Apply RX runtime settings to an already configured RX PIO SM.
 * @param pio PIO instance.
 * @param pio_sm State machine index.
 * @param polarities RX polarity settings.
 */
void rx_clock_update_settings(PIO pio, uint pio_sm, V24_RX_POLARITIES_T* polarities);

#endif /* PIO_TX_RX_DRIVER_H */
