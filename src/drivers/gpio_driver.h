/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/drivers/gpio_driver.h
 * Purpose: GPIO initialization and polarity configuration API.
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef GPIO_DRIVER_H
#define GPIO_DRIVER_H

// Related headers

// Standard library headers
#include <stdbool.h>

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

V24_POLARITIES_T init_polarities(void);
void init_pins(void);

#endif /* GPIO_DRIVER_H */
