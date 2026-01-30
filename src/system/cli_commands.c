/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/cli_commands.c
 * Purpose: CLI command handlers and dispatch.
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "cli_commands.h"

// Standard library headers
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// Library Headers
#include "hardware/gpio.h"
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"

// Project Headers
#include "board_pins.h"
#include "baudrate_monitor.h"

// Generated headers

// Pin lookup table
typedef struct {
    const char *name;
    uint gpio_num;
    bool is_output;
} pin_info_t;

static const pin_info_t pin_table[] = {
    {"txd", PIN_TXD, true},
    {"rxd", PIN_RXD, false},
    {"rts", PIN_RTS, true},
    {"cts", PIN_CTS, false},
    {"dtr", PIN_DTR, true},
    {"dsr", PIN_DSR, false},
    {"dcd", PIN_DCD, false},
    {"tx_active", PIN_TX_ACTIVE, true},
    {"led", PIN_STATUS_LED, true}
};

#define NUM_PINS (sizeof(pin_table) / sizeof(pin_table[0]))

// Command handler type
typedef void (*cmd_handler_t)(const char *args);

typedef struct {
    const char *name;
    cmd_handler_t handler;
    const char *help;
} command_t;

// Helper function to find pin by name
static const pin_info_t* find_pin(const char *name)
{
    for (size_t i = 0; i < NUM_PINS; i++)
    {
        if (strcmp(name, pin_table[i].name) == 0)
        {
            return &pin_table[i];
        }
    }
    return NULL;
}

// Command handlers
static void cmd_help(const char *args)
{
    printf("Commands: help, status, net, set <pin> <0|1>, get <pin>\r\n");
    printf("Available pins: ");
    for (size_t i = 0; i < NUM_PINS; i++)
    {
        printf("%s%s", pin_table[i].name, (i < NUM_PINS - 1) ? ", " : "\r\n");
    }
}

static void cmd_status(const char *args)
{
    printf("status: ok\r\n");
    printf("Current Baudrate estimation on pin %d: %.1f Hz\r\n",
        V24_RXC,
        baudrate_estimator_get_current_estimation(V24_RXC));

}

static void cmd_net(const char *args)
{
    wiz_NetInfo net_info;
    wizchip_getnetinfo(&net_info);
    printf("ip=%u.%u.%u.%u gw=%u.%u.%u.%u\r\n",
           net_info.ip[0], net_info.ip[1], net_info.ip[2], net_info.ip[3],
           net_info.gw[0], net_info.gw[1], net_info.gw[2], net_info.gw[3]);
}

static void cmd_set(const char *args)
{
    char pin_name[16];
    int value;

    if (sscanf(args, "%15s %d", pin_name, &value) != 2 || (value != 0 && value != 1))
    {
        printf("usage: set <pin> <0|1>\r\n");
        return;
    }

    const pin_info_t *pin = find_pin(pin_name);
    if (!pin)
    {
        printf("unknown pin: '%s'\r\n", pin_name);
        return;
    }

    if (!pin->is_output)
    {
        printf("pin '%s' is input-only\r\n", pin_name);
        return;
    }

    gpio_init(pin->gpio_num);
    gpio_set_dir(pin->gpio_num, GPIO_OUT);
    gpio_put(pin->gpio_num, value);

    printf("set %s (pin %u) = %d\r\n", pin_name, pin->gpio_num, value);
}

static void cmd_get(const char *args)
{
    char pin_name[16];

    if (sscanf(args, "%15s", pin_name) != 1)
    {
        printf("usage: get <pin>\r\n");
        return;
    }

    const pin_info_t *pin = find_pin(pin_name);
    if (!pin)
    {
        printf("unknown pin: '%s'\r\n", pin_name);
        return;
    }

    // Only initialize if not already a GPIO function
    if (gpio_get_function(pin->gpio_num) != GPIO_FUNC_SIO)
    {
        gpio_init(pin->gpio_num);
        gpio_set_dir(pin->gpio_num, GPIO_IN);
    }
    // If already GPIO but not configured as input/output, set as input
    else if (!gpio_is_dir_out(pin->gpio_num))
    {
        // Already initialized as GPIO input, no change needed
    }

    int value = gpio_get(pin->gpio_num);
    bool is_output = gpio_is_dir_out(pin->gpio_num);

    printf("get %s (pin %u) = %d [%s]\r\n", pin_name, pin->gpio_num, value, is_output ? "OUT" : "IN");
}

static void cmd_pininfo(const char *args)
{
    char pin_name[16];

    if (sscanf(args, "%15s", pin_name) != 1)
    {
        printf("usage: pininfo <pin>\r\n");
        return;
    }

    const pin_info_t *pin = find_pin(pin_name);
    if (!pin)
    {
        printf("unknown pin: '%s'\r\n", pin_name);
        return;
    }

    uint gpio_num = pin->gpio_num;
    bool is_output = gpio_is_dir_out(gpio_num);
    int value = gpio_get(gpio_num);

    printf("Pin %s (GPIO %u):\r\n", pin_name, gpio_num);
    printf("  Direction: %s\r\n", is_output ? "OUTPUT" : "INPUT");
    printf("  Value: %d\r\n", value);
    printf("  Function: %d\r\n", gpio_get_function(gpio_num));
}

// Command table
static const command_t commands[] = {
    {"help", cmd_help, "Show available commands"},
    {"status", cmd_status, "Show system status"},
    {"net", cmd_net, "Show network info"},
    {"set", cmd_set, "Set pin output: set <pin> <0|1>"},
    {"get", cmd_get, "Get pin state: get <pin>"},
    {"pininfo", cmd_pininfo, "Show pin details: pininfo <pin>"}
};

#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

void handle_cli_line(const char *line)
{
    if (line[0] == '\0') return;

    char cmd[16];
    const char *args = "";

    // Parse command and arguments
    int n = sscanf(line, "%15s", cmd);
    if (n == 1)
    {
        // Find start of arguments
        const char *space = strchr(line, ' ');
        if (space)
        {
            args = space + 1;
            while (*args == ' ') args++; // Skip leading spaces
        }
    }

    // Look up command
    for (size_t i = 0; i < NUM_COMMANDS; i++)
    {
        if (strcmp(cmd, commands[i].name) == 0)
        {
            commands[i].handler(args);
            return;
        }
    }

    printf("unknown: '%s' (try 'help')\r\n", cmd);
}
