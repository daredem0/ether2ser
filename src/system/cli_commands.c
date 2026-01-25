
// Related headers
#include "cli_commands.h"

// Standard library headers
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Library Headers
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"

// Project Headers

// Generated headers

void handle_cli_line(const char *line)
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
