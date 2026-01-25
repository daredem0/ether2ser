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
#include <string.h>

// Library Headers
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "pico/time.h"
#include "pico/types.h"
#include "hardware/gpio.h"
#include "wizchip_conf.h"
#include "w5500.h"
#include "socket.h"
#include "wizchip_spi.h"
#include "wizchip_qspi_pio.h"

// Project Headers

// Generated headers

// USB CDC enumeration delay in milliseconds
#define USB_ENUMERATION_DELAY_MS 1500

// Heartbeat message interval in milliseconds
#define HEARTBEAT_INTERVAL_MS 500

typedef enum
{
    EV_NONE = 0,
    EV_CLI_LINE,
} event_type_t;

typedef struct
{
    event_type_t type;
    union
    {
        struct
        {
            char line[128];
        } cli;
    } u;
} event_t;

#define EVQ_CAP 16
static event_t evq[EVQ_CAP];
static uint8_t evq_w = 0, evq_r = 0;

static bool evq_push(const event_t *e)
{
    uint8_t next = (uint8_t)((evq_w + 1) % EVQ_CAP);
    if (next == evq_r)
    {
        return false; // full
    }
    evq[evq_w] = *e;
    evq_w = next;
    return true;
}

static bool evq_pop(event_t *out)
{
    if (evq_r == evq_w)
    {
        return false; // empty
    }
    *out = evq[evq_r];
    evq_r = (uint8_t)((evq_r + 1) % EVQ_CAP);
    return true;
}

static void cli_poll(void)
{
    static char buf[128];
    static uint8_t len = 0;

    int c;
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT)
    {
        if (c == '\r')
        {
            continue;
        }
        if (c == '\n')
        {
            event_t e = {.type = EV_CLI_LINE};
            buf[len] = 0;
            snprintf(e.u.cli.line, sizeof(e.u.cli.line), "%s", buf);
            len = 0;
            evq_push(&e);
        }
        else if (c == 0x08 || c == 0x7F)
        { // backspace
            if (len)
            {
                len--;
            }
        }
        else if (c >= 32 && c <= 126)
        {
            if (len < sizeof(buf) - 1)
            {
                buf[len++] = (char)c;
            }
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
        wiz_NetInfo ni;
        wizchip_getnetinfo(&ni);
        printf("ip=%u.%u.%u.%u gw=%u.%u.%u.%u\r\n",
               ni.ip[0], ni.ip[1], ni.ip[2], ni.ip[3],
               ni.gw[0], ni.gw[1], ni.gw[2], ni.gw[3]);
    }
    else if (line[0] != 0)
    {
        printf("unknown: '%s' (try 'help')\r\n", line);
    }
}

int main(void)
{
    stdio_init_all();

    // Give the USB CDC a moment to enumerate (harmless even if not using USB)
    sleep_ms(USB_ENUMERATION_DELAY_MS);
    printf("v24-eth-bridge: hello from RP2040\r\n");
    printf("\r\nType 'help' in USB serial.\r\n> ");

    while (true)
    {
        cli_poll();

        event_t ev;
        while (evq_pop(&ev))
        {
            switch (ev.type)
            {
            case EV_CLI_LINE:
                handle_cli_line(ev.u.cli.line);
                printf("> ");
                break;
            default:
                break;
            }
        }
        sleep_ms(1);
    }
}
