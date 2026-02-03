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
#include "system/event_queue.h"
#include "system/persistent_config.h"

// Generated headers

// USB CDC enumeration delay in milliseconds
#define USB_ENUMERATION_DELAY_MS 1500

#define MAIN_LOOP_SLEEP_MS 1

int main(void)
{
    // Config variables
    config_t persistent_config;
    bool     config_valid = false;

    // Network variables
    UDP_CONFIG_T     local_config;
    UDP_CONFIG_T     destination_config;
    UDP_CONFIG_T     sender_config;
    NETWORK_CONFIG_T net_config;

    // Protocol and Interface variables
    V24_CONFIG_T v24_config;
    uint8_t      rx_frame_buffer_data[RX_BUF_SIZE];
    UDP_FRAME_T  rx_frame_buffer = {.length = RX_BUF_SIZE, .payload = rx_frame_buffer_data};
    uint8_t      tx_frame_buffer_data[TX_BUF_SIZE];
    UDP_FRAME_T  tx_frame_buffer = {
         .length  = TX_BUF_SIZE,
         .payload = tx_frame_buffer_data,
    };

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
        config_valid = config_read(&persistent_config);
    }

    if (config_valid)
    {
        // Setup loglevel
        set_loglevel(persistent_config.log_level);
        local_config       = (UDP_CONFIG_T)persistent_config.local_config;
        destination_config = (UDP_CONFIG_T)persistent_config.remote_config;
        v24_config         = (V24_CONFIG_T)persistent_config.v24_config;
        net_config         = (NETWORK_CONFIG_T)persistent_config.net_config;
        w5500_set_network(&net_config);
        init_v24_config(&v24_config, v24_config.baudrate);
    }
    else
    {
        // Setup loglevel
        set_loglevel(LOG_LEVEL_DEBUG);
        // Initialize Network Configuration
        local_config = (UDP_CONFIG_T){
            .ip_address = DEFAULT_IP_ADDR,
            .port       = DEFAULT_UDP_PORT,
        };
        w5500_set_network_defaults(&net_config);
        init_v24_config(&v24_config, V24_BAUD_9600);
        destination_config = (UDP_CONFIG_T){
            .port = DEFAULT_UDP_PORT,
        };
        memcpy(destination_config.ip_address, net_config.broadcast_address, 4);
    }

    w5500_open_udp_socket(&local_config);
    w5500_debug_status();

    // Initialize TX Queue
    TX_QUEUE_T tx_queue;
    uint8_t    tx_queue_buffer[TX_FRAME_QUEUE_SIZE * sizeof(TX_QUEUE_ENTRY_T)];
    tx_queue_init(&tx_queue, tx_queue_buffer);

    // Initialize HDLC Sync
    HDLC_SYNC_ACCUMULATOR_T accumulator;
    hdlc_sync_acc_init(&accumulator, HDLC_FLAG_BYTE);
    uint8_t      reconstructed_frame_buffer[RX_HDLC_SYNC_MAX_BUFFER_SIZE];
    HDLC_FRAME_T reconstructed_frame = {.payload  = reconstructed_frame_buffer,
                                        .length   = 0,
                                        .capacity = sizeof(reconstructed_frame_buffer)};

    // Initialize PIO
    // Currently anything faster than 38400 is not supported
    tx_clock_init(pio0, 0, v24_config.baudrate, &(v24_config.polarities.tx_polarities));
    rx_clock_init(pio0, 1, &(v24_config.polarities.rx_polarities));
    baudrate_estimator_init(V24_RXC);

    printf("\r\nv24-eth-bridge: hello from RP2040\r\n");
    printf("\r\nType 'help' in USB serial.\r\n> ");

    if (current_log_level == LOG_LEVEL_DEBUG)
    {
        printf("\r\nDebug logging enabled.\r\n> ");
        dump_config();
    }

    // Initialize event queue finally
    event_queue_init();
    uint8_t rx = 0;

    // Indicate DTE is present just before we enter the event loop
    gpio_put(V24_DTR, 1);

    while (true)
    {
        // Poll the event queue
        cli_poll();
        w5500_poll_rx(&sender_config, &rx_frame_buffer);
        poll_queue_stats(&tx_queue);

        // Poll the tx queue. This writes out bytes on the serial line
        tx_queue_drain(&tx_queue, 4);

        // If the tx queue is empty check if the fifo is empty to and reset rts
        if (tx_queue_is_empty(&tx_queue))
        {
            tx_poll();
        }

        // Try to read a byte and push it into the accumulator buffer
        if (rx_get(&rx))
        {
            hdlc_sync_acc_process_byte(&accumulator, rx);
        }

        // Try to get a full hdlc frame
        if (hdlc_sync_acc_poll(&accumulator, &reconstructed_frame) == E2S_ERR_HDLC_ACC_FRAME_READY)
        {
            event_t hdlc_frame_event = {.type = EV_HDLC_DECODE, .data = &reconstructed_frame};
            event_queue_push(&hdlc_frame_event);
        }
        else
        {
            // Reset state if end wasnt found. We need to hunt again
            accumulator.state          = HDLC_SYNC_STATE_HUNTING;
            reconstructed_frame.length = 0;
        }

        event_t event_item;
        for (int i = 0; i < 2 && event_queue_pop(&event_item); i++)
        {
            switch (event_item.type)
            {
            case EV_CLI_LINE:
                handle_cli_line((const char*)event_item.data);
                printf("> ");
                break;
            case EV_UDP_RX:
                LOG_DEBUG("tx_queue_enqueue_udp_frame: %d\r\n",
                          tx_queue_enqueue_udp_frame(&tx_queue, &rx_frame_buffer));
                memset(rx_frame_buffer.payload, 0, rx_frame_buffer.length);
                rx_frame_buffer.length = 0;
                printf("> ");
                break;
            case EV_UDP_TX:
                UDP_FRAME_T tx_frame = *(UDP_FRAME_T*)event_item.data;
                w5500_udp_tx(&destination_config, &tx_frame);
                printf("> ");
                break;
            case EV_HDLC_DECODE:
                HDLC_FRAME_T hdlc_frame = *(HDLC_FRAME_T*)event_item.data;
                tx_frame_buffer.length  = hdlc_frame.length;
                PRINT_FRAME_HEX("Frame: ", hdlc_frame.payload, hdlc_frame.length);
                hdlc_decode(&hdlc_frame, tx_frame_buffer.payload, TX_BUF_SIZE,
                            &(tx_frame_buffer.length));
                memset(hdlc_frame.payload, 0, hdlc_frame.length);
                hdlc_sync_acc_init(&accumulator, HDLC_FLAG_BYTE);
                event_t hdlc_frame_event = {.type = EV_UDP_TX, .data = &tx_frame_buffer};
                event_queue_push(&hdlc_frame_event);
                break;
            case EV_SAVE_CONFIG:
                LOG_INFO("Storing persistent config in flash.\r\n");
                persistent_config.log_level     = current_log_level;
                persistent_config.v24_config    = v24_config;
                persistent_config.net_config    = net_config;
                persistent_config.local_config  = local_config;
                persistent_config.remote_config = destination_config;
                config_write(&persistent_config);
                LOG_INFO("Config stored. Dumping for checking:\r\n");
                dump_config();
                printf("\r\n> ");
                break;
            default:
                break;
            }
        }
        // sleep_ms(MAIN_LOOP_SLEEP_MS);
    }
}
