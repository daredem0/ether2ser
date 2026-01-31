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
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

// Library Headers
#include "pico/stdio.h"
#include "pico/time.h"
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"

// Project Headers
#include "system/cli_commands.h"
#include "system/event_queue.h"
#include "system/cli_usb_cdc.h"
#include "system/baudrate_monitor.h"
#include "drivers/w5500_driver.h"
#include "drivers/pio_tx_driver.h"
#include "platform/pinmap.h"

// Generated headers

// USB CDC enumeration delay in milliseconds
#define USB_ENUMERATION_DELAY_MS 1500

#define MAIN_LOOP_SLEEP_MS 1


int main(void)
{
    UDP_CONFIG_T local_config = {
        .ip_address = DEFAULT_IP_ADDR,
        .port = DEFAULT_UDP_PORT,
    };
    UDP_CONFIG_T sender_config;
    NETWORK_CONFIG_T net_config;
    UDP_FRAME_T rx_frame_buffer = {
        .length = RX_BUF_SIZE,
        .payload = (uint8_t *)malloc(RX_BUF_SIZE),
    };

    stdio_init_all();
    // Give the USB CDC a moment to enumerate (harmless even if not using USB)
    sleep_ms(USB_ENUMERATION_DELAY_MS);
    w5500_driver_init();
    w5500_set_network_defaults(&net_config);
    w5500_open_udp_socket(&local_config);

    UDP_CONFIG_T destination_config = {
        .port = DEFAULT_UDP_PORT,
    };
    memcpy(destination_config.ip_address, net_config.broadcast_address, 4);
    w5500_debug_status();

    tx_clock_init(pio0, 0, V24_BAUD_4800);
    rx_clock_init(pio0, 1);
    baudrate_estimator_init(V24_RXC);

    printf("\r\nv24-eth-bridge: hello from RP2040\r\n");
    printf("\r\nType 'help' in USB serial.\r\n> ");

    event_queue_init();
    uint8_t rx = 0;

    while (true)
    {
        cli_poll();
        w5500_poll_rx(&sender_config, &rx_frame_buffer);
        tx_put(0x7E);
        if (rx_get(&rx)){
            printf("Wrote: %02X, Read: %02X\r\n", 0x7E, rx);
        }

        event_t event_item;
        while (event_queue_pop(&event_item))
        {
            switch (event_item.type)
            {
            case EV_CLI_LINE:
                handle_cli_line((const char *)event_item.data);
                printf("> ");
                break;
            case EV_UDP_RX:
                w5500_udp_tx(&destination_config, &rx_frame_buffer);
                memset(rx_frame_buffer.payload, 0, rx_frame_buffer.length);
                rx_frame_buffer.length = 0;
                printf("> ");
                break;
            default:
                break;
            }
        }
        sleep_ms(MAIN_LOOP_SLEEP_MS*50);
    }
}
