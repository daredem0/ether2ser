/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/board_pins.h
 * Purpose: GPIO pin definitions for W55RP20-EVB-PICO board
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef BOARD_PINS_H
#define BOARD_PINS_H

// Related headers

// Standard library headers

// Project Headers

// Generated headers

/**
 * @name V.24/RS-232 Interface Pins
 * @{
 */
#define PIN_TXD 0 // Serial data output to MAX3243
#define PIN_RXD 1 // Serial data input from MAX3243
#define PIN_RTS 2 // Request to Send output
#define PIN_CTS 3 // Clear to Send input
#define PIN_DTR 4 // Data Terminal Ready output
#define PIN_DSR 5 // Data Set Ready input
#define PIN_DCD 6 // Data Carrier Detect input
/** @} */

/**
 * @name Serial Clock Pins
 * @{
 */
#define PIN_TXC_EXT 15 // External transmit clock input (pin 15)
#define PIN_RXC 17     // Receive clock input
#define PIN_TXC_GEN 24 // Generated transmit clock output (pin 24)
/** @} */

/**
 * @name Control and Status Pins
 * @{
 */
#define PIN_TX_ACTIVE 25 // TX active indicator output

#define PIN_STATUS_LED PICO_DEFAULT_LED_PIN
/** @} */

#endif // BOARD_PINS_H
