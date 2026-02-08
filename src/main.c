/*
 * ether2ser — Ethernet ↔ synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/main.c
 * Purpose: Firmware entry point and high-level scheduler.
 *
 * This project targets the W55RP20-EVB-PICO (RP2040 + W5500). The RP2040 CPU
 * runs the control plane and protocol processing (L3 forwarding, framing/CRC,
 * buffering, configuration). The RP2040 PIO implements the time-critical
 * synchronous serial “PHY” (TXD/RXD with TXC/RXC).
 *
 * Notes:
 *  - USB CDC is used for a simple CLI and status output.
 *  - PIO programs live in /pio (top-level); PIO C glue lives in src/pio/.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "system/app_context.h"

// Standard library headers
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

// Library Headers
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "pico/stdio.h"
#include "pico/time.h"
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"

// Project Headers
#include "drivers/gpio_driver.h"
#include "drivers/pio_tx_rx_driver.h"
#include "drivers/tx_queue.h"
#include "drivers/w5500_driver.h"
#include "platform/pinmap.h"
#include "system/baudrate_monitor.h"
#include "system/common.h"
#include "system/event_loop.h"
#include "system/event_queue.h"
#include "system/persistent_config.h"
#include "version.h"

// Generated headers

// USB CDC enumeration delay in milliseconds
#define USB_ENUMERATION_DELAY_MS 1500

int main(void)
{
    // Application Context
    app_ctx_t app_context = {0};

    // Config variables
    config_t persistent_config;

    // Initialize USB CDC
    stdio_init_all();

    // Give the USB CDC a moment to enumerate (harmless even if not using USB)
    sleep_ms(USB_ENUMERATION_DELAY_MS);

    // Initialize W5500
    w5500_driver_init();
    // Initialize GPIOs
    init_pins();

    if (config_is_valid())
    {
        app_context.config_valid = config_read(&persistent_config);
    }
    else
    {
        app_context.config_valid = false;
    }

    init_app(&app_context, &persistent_config);

    w5500_open_udp_socket(&app_context.local_config);
    w5500_debug_status();

    // Initialize TX Queue
    tx_queue_init(&app_context.tx_queue, app_context.tx_queue_buffer);

    // Initialize PIO
    // Currently anything faster than 38400 is not supported
    tx_clock_init(pio0, 0, app_context.v24_config.baudrate,
                  &(app_context.v24_config.polarities.tx_polarities));
    rx_clock_init(pio0, 1, &(app_context.v24_config.polarities.rx_polarities));
    baudrate_estimator_init(V24_RXC);

    printf("\r\nv24-eth-bridge: hello from RP2040\r\n");
    printf("\r\nType 'help' in USB serial.\r\n");
    printf("Version: %s\r\n", VERSION_STRING);

    if (get_loglevel() == LOG_LEVEL_DEBUG)
    {
        printf("\r\nDebug logging enabled.\r\n> ");
        dump_config();
        app_context.need_prompt = true;
    }

    // Initialize event queue finally
    event_queue_init();

    // Indicate DTE is present just before we enter the event loop
    gpio_put(V24_DTR, 1);

    event_loop(&app_context);
}
