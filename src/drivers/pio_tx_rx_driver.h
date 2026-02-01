

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

// Library Headers
#include "pico/types.h"
#include "hardware/pio.h"

// Project Headers

// Generated headers


typedef struct{
    bool txd_inverted;
    bool txc_inverted;
    bool cts_inverted;
    bool rts_inverted;
    bool dtr_inverted;
} V24_TX_POLARITIES_T;

typedef struct{
    bool rxd_inverted;
    bool rxc_inverted;
    bool dcd_inverted;
} V24_RX_POLARITIES_T;

typedef struct{
    V24_TX_POLARITIES_T tx_polarities;
    V24_RX_POLARITIES_T rx_polarities;
} V24_POLARITIES_T;

typedef enum{
    V24_BAUD_1200 = 1200,
    V24_BAUD_2400 = 2400,
    V24_BAUD_4800 = 4800,
    V24_BAUD_9600 = 9600,
    V24_BAUD_19200 = 19200,
    V24_BAUD_38400 = 38400,
    V24_BAUD_57600 = 57600,
    V24_BAUD_115200 = 115200
} V24_BAUDRATE_T;

void tx_clock_init(PIO pio, uint pio_sm, V24_BAUDRATE_T baudrate, V24_TX_POLARITIES_T *polarities);
bool tx_poll(void);
bool tx_put(uint8_t data);

void rx_clock_init(PIO pio, uint pio_sm, V24_RX_POLARITIES_T *polarities);
bool rx_get(uint8_t *data);

#endif /* PIO_TX_RX_DRIVER_H */
