
/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/drivers/v24_config.h
 * Purpose: V.24 configuration types (baudrate, line polarities, runtime TX state).
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef V24_CONFIG_H
#define V24_CONFIG_H

// Related headers

// Standard library headers
#include <stdbool.h>
#include <stdint.h>

// Library Headers

// Project Headers

// Generated headers

typedef enum
{
    V24_BAUD_1200   = 1200,
    V24_BAUD_2400   = 2400,
    V24_BAUD_4800   = 4800,
    V24_BAUD_9600   = 9600,
    V24_BAUD_16000  = 16000,
    V24_BAUD_19200  = 19200,
    V24_BAUD_38400  = 38400,
    V24_BAUD_57600  = 57600,
    V24_BAUD_115200 = 115200
} V24_BAUDRATE_T;

typedef struct
{
    bool txd_inverted;
    bool txc_inverted;
    bool cts_inverted;
    bool rts_inverted;
    bool dtr_inverted;
} V24_TX_POLARITIES_T;

typedef struct
{
    bool rxd_inverted;
    bool rxc_inverted;
    bool dcd_inverted;
} V24_RX_POLARITIES_T;

typedef struct
{
    V24_TX_POLARITIES_T tx_polarities;
    V24_RX_POLARITIES_T rx_polarities;
} V24_POLARITIES_T;

typedef struct
{
    V24_BAUDRATE_T   baudrate;
    V24_POLARITIES_T polarities;
    uint32_t         tx_rts_holdoff_us;
    bool             rts_set;
} V24_CONFIG_T;

#endif /* V24_CONFIG_H */
