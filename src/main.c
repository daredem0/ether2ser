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
#include "drivers/pio_tx_rx_driver.h"
#include "platform/pinmap.h"
#include "protocol/hdlc_common.h"
#include "drivers/tx_queue.h"
#include "system/common.h"
#include "protocol/hdlc_sync.h"

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

    TX_QUEUE_T tx_queue;
    uint8_t tx_queue_buffer_data[TX_FRAME_QUEUE_SIZE * sizeof(TX_QUEUE_ENTRY_T)];
    Ringbuffer tx_queue_buffer;
    RbInit(&tx_queue_buffer, tx_queue_buffer_data, TX_FRAME_QUEUE_SIZE, sizeof(TX_QUEUE_ENTRY_T));
    tx_queue_init(&tx_queue, &tx_queue_buffer);


    HDLC_SYNC_ACCUMULATOR_T accumulator;
    hdlc_sync_acc_init(&accumulator, HDLC_FLAG_BYTE);
    uint8_t reconstructed_frame_buffer[2048];
    HDLC_FRAME_T reconstructed_frame = {
        .payload = reconstructed_frame_buffer,
        .length = 0,
        .capacity = sizeof(reconstructed_frame_buffer)
    };

    //
    stdio_init_all();

    // Initialize GPIOs
    V24_PIN_T inputs[] = {
        V24_DCD,
        V24_DSR,
        V24_CTS,
        V24_RXD,
        V24_RXC
    };
    for(size_t i = 0; i < ARRAY_LEN(inputs); i++) {
        gpio_init(inputs[i]);
        gpio_set_dir(inputs[i], GPIO_IN);
        gpio_pull_down(inputs[i]);
    }

    V24_PIN_T outputs[] = {
        V24_RTS,
        V24_TXD,
        V24_DTR,
        V24_TXC_DTE,
        V24_TXC_DCE
    };

    for(size_t i = 0; i < ARRAY_LEN(outputs); i++) {
        gpio_init(outputs[i]);
        gpio_set_dir(outputs[i], GPIO_OUT);
    }

    V24_POLARITIES_T v24_polarities = {
        .tx_polarities = {
            .txd_inverted = false,
            .txc_inverted = false,
            .cts_inverted = true,
            .rts_inverted = false,
            .dtr_inverted = false,
        },
        .rx_polarities = {
            .rxd_inverted = false,
            .rxc_inverted = false,
            .dcd_inverted = false,
        }
    };

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

    tx_clock_init(pio0, 0, V24_BAUD_4800, &v24_polarities.tx_polarities);
    rx_clock_init(pio0, 1, &v24_polarities.rx_polarities);
    baudrate_estimator_init(V24_RXC);

    printf("\r\nv24-eth-bridge: hello from RP2040\r\n");
    printf("\r\nType 'help' in USB serial.\r\n> ");

    event_queue_init();
    uint8_t rx = 0;

     // Indicate DTE is present
    gpio_set_dir(V24_DTR, GPIO_OUT);
    gpio_put(V24_DTR, 1);

    while (true)
    {
        cli_poll();
        w5500_poll_rx(&sender_config, &rx_frame_buffer);
        poll_queue_stats(&tx_queue);
        tx_queue_drain(&tx_queue, 4);
        if(tx_queue_is_empty(&tx_queue)){
            tx_poll();
        }
        if (rx_get(&rx)){
            // printf("Read: %02X\r\n", rx);
            hdlc_sync_acc_process_byte(&accumulator, rx);
        }
        if (hdlc_sync_acc_poll(&accumulator, &reconstructed_frame) == E2S_ERR_HDLC_ACC_FRAME_READY){
            printf("Frame: ");
            for(size_t i = 0; i < reconstructed_frame.length; i++){
                printf("%02X ", reconstructed_frame.payload[i]);
                if (i % 16 == 15){
                    printf("\r\n");
                }
            }
            printf("\r\n");
            printf("Length: %zu\r\n", reconstructed_frame.length);
            memset(reconstructed_frame.payload, 0, reconstructed_frame.length);
            hdlc_sync_acc_init(&accumulator, HDLC_FLAG_BYTE);
        }else{
            // Reset state if end wasnt found. We need to hunt again
            accumulator.state = HDLC_SYNC_STATE_HUNTING;
            reconstructed_frame.length = 0;
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
                printf("tx_queue_enqueue_udp_frame: %d\r\n", tx_queue_enqueue_udp_frame(&tx_queue, &rx_frame_buffer));
                // w5500_udp_tx(&destination_config, &rx_frame_buffer);
                // printf("Setting RTS\r\n");
                // gpio_put(V24_RTS, 1);
                // printf("SLeeping\r\n");
                // sleep_ms(5000);
                memset(rx_frame_buffer.payload, 0, rx_frame_buffer.length);
                rx_frame_buffer.length = 0;
                printf("> ");
                break;
            default:
                break;
            }
        }
        // sleep_ms(MAIN_LOOP_SLEEP_MS);
    }
}
