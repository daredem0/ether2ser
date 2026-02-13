
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

/**
 * @brief Supported synchronous V.24 baudrates.
 */
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

/**
 * @brief TX/control output polarity configuration.
 */
typedef struct
{
    /** Invert TXD output polarity when true. */
    bool txd_inverted;
    /** Invert TXC output polarity when true. */
    bool txc_inverted;
    /** Invert CTS input sense when true. */
    bool cts_inverted;
    /** Invert RTS output polarity when true. */
    bool rts_inverted;
    /** Invert DTR output polarity when true. */
    bool dtr_inverted;
} V24_TX_POLARITIES_T;

/**
 * @brief RX/input polarity configuration.
 */
typedef struct
{
    /** Invert RXD input sense when true. */
    bool rxd_inverted;
    /** Invert RXC input sense when true. */
    bool rxc_inverted;
    /** Invert DCD input sense when true. */
    bool dcd_inverted;
} V24_RX_POLARITIES_T;

/**
 * @brief Combined TX and RX polarity configuration.
 */
typedef struct
{
    V24_TX_POLARITIES_T tx_polarities;
    V24_RX_POLARITIES_T rx_polarities;
} V24_POLARITIES_T;

/**
 * @brief Runtime V.24 configuration and TX holdoff state.
 */
typedef struct
{
    /** Configured serial baudrate. */
    V24_BAUDRATE_T baudrate;
    /** Signal polarity set for TX and RX paths. */
    V24_POLARITIES_T polarities;
    /** RTS release holdoff in microseconds after TX completion. */
    uint32_t tx_rts_holdoff_us;
    /** Internal state: current RTS asserted/deasserted status. */
    bool rts_set;
    /** True if XCK, false if TCK */
    bool external_clock;
} V24_CONFIG_T;

#endif /* V24_CONFIG_H */
