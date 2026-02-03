/*
 * ether2ser — Ethernet ↔ synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/examples/blink/blink_pio.c
 * Purpose: PIO helper to drive a GPIO blink state machine for the example.
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers

// Standard library headers

// Project Headers
#include "hardware/pio.h"
#include "pico/types.h"

// Generated headers
#include "led_blink.pio.h"

void start_pio_led_blink(PIO pio, uint sm, uint pin)
{
    // Load PIO program
    uint offset = pio_add_program(pio, &led_blink_program);

    // Route GPIO to PIO
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);

    // Configure state machine
    pio_sm_config c = led_blink_program_get_default_config(offset);
    sm_config_set_set_pins(&c, pin, 1);

    // Slow down the state machine clock so blinking is visible.
    // 125 MHz / 65535 / 64 cycles ≈ 30 Hz toggle rate (15 Hz blink)
    // For slower: increase delay in PIO program [31] -> larger value
    sm_config_set_clkdiv(&c, 65535.0f);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}
