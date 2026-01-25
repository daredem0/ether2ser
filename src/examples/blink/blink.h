/*
 * ether2ser — Ethernet ↔ synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/examples/blink/blink.h
 * Purpose: Interfaces for the blink example (PIO and APU helpers).
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef EXAMPLES_BLINK_H
#define EXAMPLES_BLINK_H

// Related headers

// Standard library headers
#include <stdio.h>

// Library Headers
#include "pico/stdio.h"
#include "pico/time.h"
#include "pico/types.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"

// Project Headers

// Generated headers
#include "led_blink.pio.h"

/**
 * @brief Start PIO-driven LED blink on a GPIO.
 * @param pio PIO instance to use (e.g. pio0).
 * @param sm  State machine index within the PIO.
 * @param pin GPIO number to blink.
 */
void start_pio_led_blink(PIO pio, uint sm, uint pin);

/**
 * @brief Start CPU-driven LED blink on a GPIO.
 * @param pin GPIO number to blink.
 */
void start_cpu_led_blink(uint pin);

#endif /* EXAMPLES_BLINK_H */
