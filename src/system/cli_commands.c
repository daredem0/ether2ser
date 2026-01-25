
// Related headers
#include "cli_commands.h"

// Standard library headers
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Library Headers
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"

// Project Headers
#include "board_pins.h"

// Generated headers

void handle_cli_line(const char *line)
{
    if (strcmp(line, "help") == 0)
    {
        printf("Commands: help, status, net, set <pin> <0|1>\r\n");
        printf("Set Pins: txd, rts, dtr, tx_active, led\r\n");
        printf("Get Pins: rxd, cts, dsr, dcd\r\n");
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
    else if (strncmp(line, "set ", 4) == 0)
    {
        char pin_name[16];
        int value;
        int parsed = sscanf(line + 4, "%15s %d", pin_name, &value);

        if (parsed != 2 || (value != 0 && value != 1))
        {
            printf("usage: set <pin> <0|1>\r\n");
            printf("pins: txd, rts, dtr, tx_active, led\r\n");
            return;
        }

        uint pin_num = 0;
        bool pin_found = false;

        if (strcmp(pin_name, "txd") == 0)
        {
            pin_num = PIN_TXD;
            pin_found = true;
        }
        else if (strcmp(pin_name, "rts") == 0)
        {
            pin_num = PIN_RTS;
            pin_found = true;
        }
        else if (strcmp(pin_name, "dtr") == 0)
        {
            pin_num = PIN_DTR;
            pin_found = true;
        }
        else if (strcmp(pin_name, "tx_active") == 0)
        {
            pin_num = PIN_TX_ACTIVE;
            pin_found = true;
        }
        else if (strcmp(pin_name, "led") == 0)
        {
            pin_num = PIN_STATUS_LED;
            pin_found = true;
        }

        if (!pin_found)
        {
            printf("unknown pin: '%s'\r\n", pin_name);
            printf("valid pins: txd, rts, dtr, tx_active, led\r\n");
            return;
        }

        // Initialize and configure pin as output
        gpio_init(pin_num);
        gpio_set_dir(pin_num, GPIO_OUT);
        gpio_put(pin_num, value);

        printf("set %s (pin %u) = %d\r\n", pin_name, pin_num, value);
    }
    else if (strncmp(line, "get ", 4) == 0){

        char pin_name[16];
        int value;
        int parsed = sscanf(line + 4, "%15s", pin_name);

        if (parsed != 1)
        {
            printf("usage: get <pin>\r\n");
            printf("pins: txd, rxd, rts, cts, dtr, dsr, dcd, led\r\n");
            return;
        }

        uint pin_num = 0;
        bool pin_found = false;


        if (strcmp(pin_name, "rxd") == 0)
        {
            pin_num = PIN_RXD;
            pin_found = true;
        }
        else if (strcmp(pin_name, "cts") == 0)
        {
            pin_num = PIN_CTS;
            pin_found = true;
        }
        else if (strcmp(pin_name, "dsr") == 0)
        {
            pin_num = PIN_DSR;
            pin_found = true;
        }
        else if (strcmp(pin_name, "dcd") == 0)
        {
            pin_num = PIN_DCD;
            pin_found = true;
        }
        else if (strcmp(pin_name, "txd") == 0)
        {
            pin_num = PIN_TXD;
            pin_found = true;
        }
        else if (strcmp(pin_name, "rts") == 0)
        {
            pin_num = PIN_RTS;
            pin_found = true;
        }
        else if (strcmp(pin_name, "dtr") == 0)
        {
            pin_num = PIN_DTR;
            pin_found = true;
        }
        else if (strcmp(pin_name, "tx_active") == 0)
        {
            pin_num = PIN_TX_ACTIVE;
            pin_found = true;
        }
        else if (strcmp(pin_name, "led") == 0)
        {
            pin_num = PIN_STATUS_LED;
            pin_found = true;
        }

        if (!pin_found)
        {
            printf("unknown pin: '%s'\r\n", pin_name);
            printf("valid pins: rxd, cts, dsr, dcd\r\n");
            return;
        }

        // Initialize and configure pin as output
        gpio_init(pin_num);
        gpio_set_dir(pin_num, GPIO_IN);
        value = gpio_get(pin_num);

        printf("get %s (pin %u) = %d\r\n", pin_name, pin_num, value);
    }
    else if (line[0] != '\0')
    {
        printf("unknown: '%s' (try 'help')\r\n", line);
    }
}
