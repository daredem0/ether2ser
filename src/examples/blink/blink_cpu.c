/*
 * ether2ser — Ethernet ↔ synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/examples/blink/blink_apu.c
 * Purpose: CPU-side helper to blink a GPIO for the blink example.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers

// Standard library headers
#include <stdio.h>

// Project Headers
#include "hardware/pio.h"
#include "pico/stdio.h"
#include "pico/time.h"
#include "pico/types.h"

// Generated headers
#include "led_blink.pio.h"

void start_cpu_led_blink(uint pin)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);

    bool on = false;
    while (true)
    {
        on = !on;
        gpio_put(pin, on);
        printf("tick: led=%d\r\n", on ? 1 : 0);
        sleep_ms(500);
    }
}
