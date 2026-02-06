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
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "system/app_context.h"

// Standard library headers
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Library Headers
#include "pico/stdio.h"
#include "pico/time.h"
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"

// Project Headers
#include "drivers/pio_tx_rx_driver.h"
#include "drivers/tx_queue.h"
#include "drivers/w5500_driver.h"
#include "platform/pinmap.h"
#include "protocol/hdlc_common.h"
#include "protocol/hdlc_decoder.h"
#include "protocol/hdlc_sync.h"
#include "system/baudrate_monitor.h"
#include "system/cli_commands.h"
#include "system/cli_usb_cdc.h"
#include "system/common.h"
#include "system/event_loop.h"
#include "system/event_queue.h"
#include "system/persistent_config.h"

// Generated headers

// USB CDC enumeration delay in milliseconds
#define USB_ENUMERATION_DELAY_MS 1500

int main(void)
{
    // Application Context
    app_ctx_t app_context = {0};

    // Config variables
    config_t persistent_config;
    // bool     config_valid = false;

    // // Network variables
    // UDP_CONFIG_T     local_config;
    // UDP_CONFIG_T     destination_config;
    // UDP_CONFIG_T     sender_config;
    // NETWORK_CONFIG_T net_config;

    // // Protocol and Interface variables
    // V24_CONFIG_T v24_config;
    // uint8_t      rx_frame_buffer_data[RX_BUF_SIZE];
    // UDP_FRAME_T  rx_frame_buffer = {.length = RX_BUF_SIZE, .payload = rx_frame_buffer_data};
    // uint8_t      tx_frame_buffer_data[TX_BUF_SIZE];
    // UDP_FRAME_T  tx_frame_buffer = {
    //      .length  = TX_BUF_SIZE,
    //      .payload = tx_frame_buffer_data,
    // };

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

    // if (config_valid)
    // {
    //     // Setup loglevel
    //     set_loglevel(persistent_config.log_level);
    //     local_config       = (UDP_CONFIG_T)persistent_config.local_config;
    //     destination_config = (UDP_CONFIG_T)persistent_config.remote_config;
    //     v24_config         = (V24_CONFIG_T)persistent_config.v24_config;
    //     net_config         = (NETWORK_CONFIG_T)persistent_config.net_config;
    //     w5500_set_network(&net_config);
    //     init_v24_config(&v24_config, v24_config.baudrate);
    // }
    // else
    // {
    //     // Setup loglevel
    //     set_loglevel(LOG_LEVEL_DEBUG);
    //     // Initialize Network Configuration
    //     local_config = (UDP_CONFIG_T){
    //         .ip_address = DEFAULT_IP_ADDR,
    //         .port       = DEFAULT_UDP_PORT,
    //     };
    //     w5500_set_network_defaults(&net_config);
    //     init_v24_config(&v24_config, V24_BAUD_9600);
    //     destination_config = (UDP_CONFIG_T){
    //         .port = DEFAULT_UDP_PORT,
    //     };
    //     memcpy(destination_config.ip_address, net_config.broadcast_address, 4);
    // }

    w5500_open_udp_socket(&app_context.local_config);
    w5500_debug_status();

    // Initialize TX Queue
    // TX_QUEUE_T tx_queue;
    // uint8_t    tx_queue_buffer[TX_FRAME_QUEUE_SIZE * sizeof(TX_QUEUE_ENTRY_T)];
    tx_queue_init(&app_context.tx_queue, app_context.tx_queue_buffer);

    // Initialize HDLC Sync
    HDLC_SYNC_ACCUMULATOR_T accumulator;
    hdlc_sync_acc_init(&accumulator, HDLC_FLAG_BYTE);
    // uint8_t      reconstructed_frame_buffer[RX_HDLC_SYNC_MAX_BUFFER_SIZE];
    // HDLC_FRAME_T reconstructed_frame = {.payload  = reconstructed_frame_buffer,
    //                                     .length   = 0,
    //                                     .capacity = sizeof(reconstructed_frame_buffer)};

    // Initialize PIO
    // Currently anything faster than 38400 is not supported
    tx_clock_init(pio0, 0, app_context.v24_config.baudrate,
                  &(app_context.v24_config.polarities.tx_polarities));
    rx_clock_init(pio0, 1, &(app_context.v24_config.polarities.rx_polarities));
    baudrate_estimator_init(V24_RXC);

    printf("\r\nv24-eth-bridge: hello from RP2040\r\n");
    printf("\r\nType 'help' in USB serial.\r\n> ");

    if (current_log_level == LOG_LEVEL_DEBUG)
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
