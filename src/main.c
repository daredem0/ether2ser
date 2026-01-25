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
#include <string.h>

// Library Headers
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "pico/time.h"
#include "pico/error.h"
#include "hardware/gpio.h"
#include "wizchip_conf.h"
#include "w5500.h"
#include "socket.h"
#include "wizchip_spi.h"
#include "wizchip_qspi_pio.h"
#include "system/event_queue.h"

// Project Headers

// Generated headers

// USB CDC enumeration delay in milliseconds
#define USB_ENUMERATION_DELAY_MS 1500

// Heartbeat message interval in milliseconds
#define HEARTBEAT_INTERVAL_MS 500
#define CLI_BUFFER_SIZE 128
#define CLI_EVENT_POOL_SIZE EVENT_QUEUE_CAPACITY
#define ASCII_BACKSPACE 0x08
#define ASCII_DELETE 0x7F
#define ASCII_FIRST_PRINTABLE 32
#define ASCII_LAST_PRINTABLE 126
#define MAIN_LOOP_SLEEP_MS 1

static void cli_poll(void)
{
    static char line_buffer[CLI_BUFFER_SIZE];
    static uint8_t buffer_len = 0;
    static char cli_line_pool[CLI_EVENT_POOL_SIZE][CLI_BUFFER_SIZE];
    static uint8_t cli_line_pool_write = 0;

    int input_char;
    while ((input_char = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT)
    {

        if (input_char == '\n' || input_char == '\r')
        {
            // add new line before processing command
            printf("\r\n");
            line_buffer[buffer_len] = '\0';

            char *line_slot = cli_line_pool[cli_line_pool_write];
            int written = snprintf(line_slot, CLI_BUFFER_SIZE, "%s", line_buffer);
            if (written < 0)
            {
                printf("[WARN] failed to format CLI line\r\n");
                buffer_len = 0;
                continue;
            }
            if (written >= CLI_BUFFER_SIZE)
            {
                printf("[WARN] CLI line truncated (len=%d)\r\n", written);
                buffer_len = 0;
                continue;
            }

            event_t new_event = {
                .type = EV_CLI_LINE,
                .data = line_slot,
                .data_len = strnlen(line_slot, CLI_BUFFER_SIZE) + 1,
            };

            if (event_queue_push(&new_event))
            {
                cli_line_pool_write = (uint8_t)((cli_line_pool_write + 1) % CLI_EVENT_POOL_SIZE);
            }
            else
            {
                printf("[WARN] event queue full, dropping line\r\n");
            }

            buffer_len = 0;
        }
        else if (input_char == ASCII_BACKSPACE || input_char == ASCII_DELETE)
        { // backspace
            printf("\b \b");
            if (buffer_len)
            {
                buffer_len--;
            }
        }
        else if (input_char >= ASCII_FIRST_PRINTABLE && input_char <= ASCII_LAST_PRINTABLE)
        {
            if (buffer_len < sizeof(line_buffer) - 1)
            {
                // Echo character back and sore it in buffer
                printf("%c", (char)input_char);
                line_buffer[buffer_len++] = (char)input_char;
            }
        }
        else
        {
            printf("[DEBUG] Ignoring char: 0x%02X\r\n", input_char);
        }
    }
}

static void handle_cli_line(const char *line)
{
    if (strcmp(line, "help") == 0)
    {
        printf("Commands: help, status, net\r\n");
    }
    else if (strcmp(line, "status") == 0)
    {
        printf("status: ok\r\n");
    }
    else if (strcmp(line, "net") == 0)
    {
        wiz_NetInfo net_info;
        wizchip_getnetinfo(&net_info);
        printf("ip=%u.%u.%u.%u gw=%u.%u.%u.%u\r\n",
               net_info.ip[0], net_info.ip[1], net_info.ip[2], net_info.ip[3],
               net_info.gw[0], net_info.gw[1], net_info.gw[2], net_info.gw[3]);
    }
    else if (line[0] != '\0')
    {
        printf("unknown: '%s' (try 'help')\r\n", line);
    }
}

int main(void)
{
    stdio_init_all();
    event_queue_init();

    // Give the USB CDC a moment to enumerate (harmless even if not using USB)
    sleep_ms(USB_ENUMERATION_DELAY_MS);
    printf("v24-eth-bridge: hello from RP2040\r\n");
    printf("\r\nType 'help' in USB serial.\r\n> ");

    while (true)
    {
        cli_poll();

        event_t event_item;
        while (event_queue_pop(&event_item))
        {
            switch (event_item.type)
            {
            case EV_CLI_LINE:
                handle_cli_line((const char *)event_item.data);
                printf("> ");
                break;
            default:
                break;
            }
        }
        sleep_ms(MAIN_LOOP_SLEEP_MS);
    }
}
