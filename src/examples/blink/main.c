/*
 * ether2ser — Ethernet ↔ synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/examples/blink/main.c
 * Purpose: Example entry point blinking the USER LED via PIO and APU.
 *
 * Notes:
 *  - Demonstrates concurrent PIO-driven and CPU-driven GPIO blinking.
 *  - Targets the W55RP20-EVB-PICO (USER LED on GPIO19).
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "blink.h"

// Standard library headers
#include <stdio.h>

// Library Headers
#include "pico/stdio.h"
#include "pico/time.h"
#include "hardware/pio.h"

// Project Headers

// Generated headers

#define APU_LED_PIN 25

// W55RP20-EVB-PICO onboard USER LED is on GPIO19.
#define PIO_LED_PIN 19

int main(void)
{
    stdio_init_all();

    // Give the USB CDC a moment to enumerate (harmless even if not using USB)
    sleep_ms(1500);

    printf("v24-eth-bridge: hello from RP2040\r\n");
    printf("PIO blinking GPIO19 (USER LED), APU blinking GPIO%d\r\n", APU_LED_PIN);

    // --- Start PIO blink on onboard USER LED (GPIO19) ---
    start_pio_led_blink(pio0, 0, PIO_LED_PIN);

    // --- Keep your existing APU blink code unchanged ---
    start_cpu_led_blink(APU_LED_PIN);
}
