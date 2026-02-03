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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Library Headers
#include "hardware/gpio.h"
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"

// Project Headers
#include "baudrate_monitor.h"
#include "board_pins.h"
#include "cli_parser.h"
#include "error.h"
#include "event_queue.h"
#include "persistent_config.h"
// Generated headers

#define MAX_CMD_BUFFER_LEN 16
#define MAX_PIN_NAME_LEN 16
#define MAX_ARG_BUFFER_LEN 64

// Command handler type
typedef void (*cmd_handler_t)(const char* args);

typedef struct
{
    const char*   name;
    cmd_handler_t handler;
    const char*   help;
} command_t;

typedef void (*category_set_handler_t)(const char* args);
typedef void (*category_get_handler_t)(const char* args);

typedef struct
{
    const char*            name;
    category_set_handler_t set_handler;
    category_get_handler_t get_handler;
} category_t;

typedef void (*subcmd_set_handler_t)(const char* args);
typedef void (*subcmd_get_handler_t)(const char* args);

typedef struct
{
    const char*          name;
    subcmd_set_handler_t set_handler;
    subcmd_get_handler_t get_handler;
} subcmd_t;

// Forward declarations for lookup tables
static void cmd_help(const char* args);
static void cmd_status(const char* args);
static void cmd_net(const char* args);
static void cmd_set(const char* args);
static void cmd_get(const char* args);
static void cmd_pininfo(const char* args);
static void cmd_save(const char* args);
static void cmd_set_ip(const char* args);
static void cat_gpio_set(const char* args);
static void cat_gpio_get(const char* args);
static void cat_net_set(const char* args);
static void cat_net_get(const char* args);
static void subcmd_set_ip_local(const char* args);
static void subcmd_get_ip_local(const char* args);
static void subcmd_set_ip_remote(const char* args);
static void subcmd_get_ip_remote(const char* args);
static void subcmd_set_ip_gateway(const char* args);
static void subcmd_get_ip_gateway(const char* args);
static void subcmd_set_udp_port_local(const char* args);
static void subcmd_get_udp_port_local(const char* args);
static void subcmd_set_udp_port_remote(const char* args);
static void subcmd_get_udp_port_remote(const char* args);

// Lookup tables
static const command_t commands[] = {
    {"help", cmd_help, "Show available commands"},
    {"status", cmd_status, "Show system status"},
    {"save", cmd_save, "Save configuration"},
    {"net", cmd_net, "Show network info"},
    {"set", cmd_set, "Set values: set gpio <pin> <0|1> | set net ip <addr>/<cidr>"},
    {"get", cmd_get, "Get pin state: get <pin>"},
    {"pininfo", cmd_pininfo, "Show pin details: pininfo <pin>"}};

static const category_t categories[] = {
    {"gpio", cat_gpio_set, cat_gpio_get},
    {"net", cat_net_set, cat_net_get},
};

static const subcmd_t net_subcmds[] = {
    {"ip.local", subcmd_set_ip_local, subcmd_get_ip_local},
    {"ip.remote", subcmd_set_ip_remote, subcmd_get_ip_remote},
    {"gateway", subcmd_set_ip_gateway, subcmd_get_ip_gateway},
    {"udp.port.local", subcmd_set_udp_port_local, subcmd_get_udp_port_local},
    {"udp.port.remote", subcmd_set_udp_port_remote, subcmd_get_udp_port_remote},
};

#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))
#define NUM_CATEGORIES ARRAY_LEN(categories)
#define NUM_NET_SUBCMDS ARRAY_LEN(net_subcmds)

static void subcmd_get_ip_local(const char* args)
{
    wiz_NetInfo net_info;
    wizchip_getnetinfo(&net_info);
    printf("ip=%u.%u.%u.%u sn=%u.%u.%u.%u\r\n", net_info.ip[0], net_info.ip[1], net_info.ip[2],
           net_info.ip[3], net_info.sn[0], net_info.sn[1], net_info.sn[2], net_info.sn[3]);
}

static void subcmd_set_ip_local(const char* args)
{
    uint8_t ip[4], mask[4];
    if (parse_set_ip_args(args, ip, mask) != E2S_OK)
    {
        printf("usage: set net ip 192.168.29.2/24\r\n");
        return;
    }

    // Example: read, modify, write live config
    wiz_NetInfo net_info;
    wizchip_getnetinfo(&net_info);
    memcpy(net_info.ip, ip, 4);
    memcpy(net_info.sn, mask, 4);
    wizchip_setnetinfo(&net_info);
    printf("ip=%u.%u.%u.%u sn=%u.%u.%u.%u\r\n", ip[0], ip[1], ip[2], ip[3], mask[0], mask[1],
           mask[2], mask[3]);
}

static void subcmd_get_ip_remote(const char* args)
{
    (void)args;
    printf("ip.remote: not implemented yet\r\n");
}

static void subcmd_set_ip_remote(const char* args)
{
    (void)args;
    printf("set ip.remote: not implemented yet (args='%s')\r\n", args);
}

static void subcmd_get_ip_gateway(const char* args)
{
    (void)args;
    printf("gateway: not implemented yet\r\n");
}

static void subcmd_set_ip_gateway(const char* args)
{
    (void)args;
    printf("set gateway: not implemented yet (args='%s')\r\n", args);
}

static void subcmd_get_udp_port_local(const char* args)
{
    (void)args;
    printf("udp.port.local: not implemented yet\r\n");
}

static void subcmd_set_udp_port_local(const char* args)
{
    (void)args;
    printf("set udp.port.local: not implemented yet (args='%s')\r\n", args);
}

static void subcmd_get_udp_port_remote(const char* args)
{
    (void)args;
    printf("udp.port.remote: not implemented yet\r\n");
}

static void subcmd_set_udp_port_remote(const char* args)
{
    (void)args;
    printf("set udp.port.remote: not implemented yet (args='%s')\r\n", args);
}

static void cat_net_get(const char* args)
{
    LOG_DEBUG("get net: args='%s'\r\n", args);
    for (size_t i = 0; i < NUM_NET_SUBCMDS; i++)
    {
        size_t len = strlen(net_subcmds[i].name);
        if (strncmp(args, net_subcmds[i].name, len) == 0 && (args[len] == ' ' || args[len] == '\0'))
        {
            const char* sub_args = args[len] == ' ' ? args + len + 1 : "";
            net_subcmds[i].get_handler(sub_args);
            return;
        }
    }
}

static void cat_gpio_set(const char* args)
{
    char              pin_name[MAX_PIN_NAME_LEN];
    int               value;
    const pin_info_t* pin           = NULL;
    e2s_error_t       parser_result = parse_set_gpio_args(args, pin_name, &value, &pin);

    switch (parser_result)
    {
    case E2S_ERR_CLI_USAGE_SET:
        printf("usage:\r\n");
        printf("  set gpio <pin> <0|1>\r\n");
        return;
    case E2S_ERR_CLI_UNKNOWN_PIN:
        printf("unknown pin: '%s'\r\n", pin_name);
        return;
    case E2S_ERR_CLI_PIN_INPUT_ONLY:
        printf("pin '%s' is input-only\r\n", pin_name);
        return;
    case E2S_OK:
        break;
    default:
        // Unreachable
        return;
    }

    if (pin == NULL)
    {
        return;
    }

    gpio_init(pin->gpio_num);
    gpio_set_dir(pin->gpio_num, GPIO_OUT);
    gpio_put(pin->gpio_num, value);

    printf("set %s (pin %u) = %d\r\n", pin_name, pin->gpio_num, value);
}

static void cat_net_set(const char* args)
{
    LOG_DEBUG("net set: '%s'\r\n", args);
    for (size_t i = 0; i < NUM_NET_SUBCMDS; i++)
    {
        size_t len = strlen(net_subcmds[i].name);
        if (strncmp(args, net_subcmds[i].name, len) == 0 && (args[len] == ' ' || args[len] == '\0'))
        {
            const char* sub_args = args[len] == ' ' ? args + len + 1 : "";
            net_subcmds[i].set_handler(sub_args);
            return;
        }
    }
    // print usage
}

static void cat_gpio_get(const char* args)
{
    char              pin_name[MAX_PIN_NAME_LEN];
    const pin_info_t* pin = NULL;

    e2s_error_t parser_result = parse_get_args(args, pin_name, &pin);

    switch (parser_result)
    {
    case E2S_ERR_CLI_USAGE_GET:
        printf("usage: get <pin>\r\n");
        return;
    case E2S_ERR_CLI_UNKNOWN_PIN:
        printf("unknown pin: '%s'\r\n", pin_name);
        return;
    case E2S_OK:
        break;
    default:
        // Unreachable
        return;
    }

    if (pin == NULL)
    {
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

    int  value     = gpio_get(pin->gpio_num);
    bool is_output = gpio_is_dir_out(pin->gpio_num);

    printf("get %s (pin %u) = %d [%s]\r\n", pin_name, pin->gpio_num, value,
           is_output ? "OUT" : "IN");
}

static void cmd_set(const char* args)
{
    for (size_t i = 0; i < NUM_CATEGORIES; i++)
    {
        size_t len = strlen(categories[i].name);
        if (strncmp(args, categories[i].name, len) == 0 && args[len] == ' ')
        {
            categories[i].set_handler(args + len + 1);
            return;
        }
    }
    // print usage
}

static void cmd_get(const char* args)
{
    for (size_t i = 0; i < NUM_CATEGORIES; i++)
    {
        size_t len = strlen(categories[i].name);
        if (strncmp(args, categories[i].name, len) == 0 && args[len] == ' ')
        {
            categories[i].get_handler(args + len + 1);
            return;
        }
    }
    // print usage
}

static void cmd_set_ip(const char* args)
{
    uint8_t ip[4], mask[4];
    if (parse_set_ip_args(args, ip, mask) != E2S_OK)
    {
        printf("usage: set net ip 192.168.29.2/24\r\n");
        return;
    }

    // Example: read, modify, write live config
    wiz_NetInfo net_info;
    wizchip_getnetinfo(&net_info);
    memcpy(net_info.ip, ip, 4);
    memcpy(net_info.sn, mask, 4);
    wizchip_setnetinfo(&net_info);
    printf("ip=%u.%u.%u.%u sn=%u.%u.%u.%u\r\n", ip[0], ip[1], ip[2], ip[3], mask[0], mask[1],
           mask[2], mask[3]);
}

static void cmd_save(const char* args)
{
    (void)args;
    event_t save_event = {.type = EV_SAVE_CONFIG, .data = NULL, .data_len = 0};
    event_queue_push(&save_event);
}

// Command handlers
static void cmd_help(const char* args)
{
    (void)args;

    size_t max_cmd = 0;
    for (size_t i = 0; i < NUM_COMMANDS; i++)
    {
        size_t len = strlen(commands[i].name);
        if (len > max_cmd)
        {
            max_cmd = len;
        }
    }

    printf("\r\nCommands:\r\n");
    for (size_t i = 0; i < NUM_COMMANDS; i++)
    {
        printf("  %-*s  %s\r\n", (int)max_cmd, commands[i].name, commands[i].help);
    }

    printf("\r\nPins:\r\n");
    const pin_info_t* pin_table = get_pin_table();
    for (size_t i = 0; i < NUM_PINS; i++)
    {
        printf("  %-10s  %s\r\n", pin_table[i].name, pin_table[i].is_output ? "OUT" : "IN");
    }
    printf("\r\n");
}

static void cmd_status(const char* args)
{
    printf("status: ok\r\n");
    printf("Current Baudrate estimation on pin %d: %.1f Hz\r\n", V24_RXC,
           baudrate_estimator_get_current_estimation(V24_RXC));
}

static void cmd_net(const char* args)
{
    wiz_NetInfo net_info;
    wizchip_getnetinfo(&net_info);
    printf("ip=%u.%u.%u.%u gw=%u.%u.%u.%u\r\n", net_info.ip[0], net_info.ip[1], net_info.ip[2],
           net_info.ip[3], net_info.gw[0], net_info.gw[1], net_info.gw[2], net_info.gw[3]);
}

static void cmd_pininfo(const char* args)
{
    char pin_name[MAX_PIN_NAME_LEN];

    if (sscanf(args, "%15s", pin_name) != 1)
    {
        printf("usage: pininfo <pin>\r\n");
        return;
    }

    const pin_info_t* pin = find_pin(pin_name);
    if (!pin)
    {
        printf("unknown pin: '%s'\r\n", pin_name);
        return;
    }

    uint gpio_num  = pin->gpio_num;
    bool is_output = gpio_is_dir_out(gpio_num);
    int  value     = gpio_get(gpio_num);

    printf("Pin %s (GPIO %u):\r\n", pin_name, gpio_num);
    printf("  Direction: %s\r\n", is_output ? "OUTPUT" : "INPUT");
    printf("  Value: %d\r\n", value);
    printf("  Function: %d\r\n", gpio_get_function(gpio_num));
}

const char* get_command_name(int index)
{
    return commands[index].name;
}

void handle_cli_line(const char* line)
{

    char cmd[MAX_CMD_BUFFER_LEN];
    cmd[0] = '\0';
    char args[MAX_ARG_BUFFER_LEN];
    if (cli_parse(line, cmd, args) == E2S_OK)
    {
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
}
