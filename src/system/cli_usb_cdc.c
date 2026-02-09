/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/cli_usb_cdc.c
 * Purpose: USB CDC CLI input handling and event dispatch.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "cli_usb_cdc.h"

// Standard library headers
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Library Headers
#include "pico/error.h"
#include "pico/stdio.h"

// Project Headers
#include "system/common.h"
#include "system/event_queue.h"

// Generated headers

// Heartbeat message interval in milliseconds
#define CLI_BUFFER_SIZE 128
#define CLI_EVENT_POOL_SIZE EVENT_QUEUE_CAPACITY
#define ASCII_BACKSPACE 0x08
#define ASCII_DELETE 0x7F
#define ASCII_FIRST_PRINTABLE 32
#define ASCII_LAST_PRINTABLE 126
#define CLI_MAX_CHARS_PER_POLL 16U

void cli_poll(void)
{
    static char    line_buffer[CLI_BUFFER_SIZE];
    static uint8_t buffer_len = 0;
    static char    cli_line_pool[CLI_EVENT_POOL_SIZE][CLI_BUFFER_SIZE];
    static uint8_t cli_line_pool_write = 0;

    int      input_char;
    uint32_t processed_chars = 0U;
    while (processed_chars < CLI_MAX_CHARS_PER_POLL &&
           (input_char = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT)
    {
        processed_chars++;

        if (input_char == '\n' || input_char == '\r')
        {
            // add new line before processing command
            LOG_PLAIN("\r\n");
            line_buffer[buffer_len] = '\0';

            char* line_slot = cli_line_pool[cli_line_pool_write];
            int   written   = snprintf(line_slot, CLI_BUFFER_SIZE, "%s", line_buffer);
            if (written < 0)
            {
                LOG_ERROR("Failed to format CLI line\r\n");
                buffer_len = 0;
                continue;
            }
            if (written >= CLI_BUFFER_SIZE)
            {
                LOG_ERROR("CLI line truncated (len=%d)\r\n", written);
                buffer_len = 0;
                continue;
            }

            event_t new_event = {
                .type     = EV_CLI_LINE,
                .data     = {line_slot},
                .data_len = strnlen(line_slot, CLI_BUFFER_SIZE) + 1,
            };

            if (event_queue_push(&new_event))
            {
                cli_line_pool_write = (uint8_t)((cli_line_pool_write + 1) % CLI_EVENT_POOL_SIZE);
            }
            else
            {
                LOG_ERROR("Event queue full, dropping line\r\n");
            }

            buffer_len = 0;
        }
        else if (input_char == ASCII_BACKSPACE || input_char == ASCII_DELETE)
        { // backspace
            LOG_PLAIN("\b \b");
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
                LOG_PLAIN("%c", (char)input_char);
                line_buffer[buffer_len++] = (char)input_char;
            }
        }
        else
        {
            LOG_DEBUG("[DEBUG] Ignoring char: 0x%02X\r\n", input_char);
        }
    }
}
