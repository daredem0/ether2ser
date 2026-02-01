
// Related headers
#include "cli_parser.h"

// Standard library headers
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

// Library Headers

// Project Headers
#include "error.h"
#include "platform/pinmap.h"

// Generated headers


static const pin_info_t pin_table[] = {
    {"txd", V24_TXD, true},
    {"rxd", V24_RXD, false},
    {"rts", V24_RTS, true},
    {"cts", V24_CTS, false},
    {"dtr", V24_DTR, true},
    {"dsr", V24_DSR, false},
    {"dcd", V24_DCD, false},
    {"tx_active", V24_TX_ACTIVE, true},
    {"led", V24_STATUS_LED, true}
};

const pin_info_t* get_pin_table(void){
    return pin_table;
}

// Helper function to find pin by name
const pin_info_t* find_pin(const char *name)
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

e2s_error_t parse_set_args(const char *args, char *pin_name, int *value, const pin_info_t **pin){
    if (sscanf(args, "%15s %d", pin_name, value) != 2 || (*value != 0 && *value != 1))
    {
        return E2S_ERR_CLI_USAGE_SET;
    }

    *pin = find_pin(pin_name);
    if (!*pin)
    {
        return E2S_ERR_CLI_UNKNOWN_PIN;
    }

    if (!(*pin)->is_output)
    {
        return E2S_ERR_CLI_PIN_INPUT_ONLY;
    }
    return E2S_OK;
}

e2s_error_t parse_get_args(const char *args, char *pin_name, const pin_info_t **pin){

    if (sscanf(args, "%15s", pin_name) != 1)
    {
        return E2S_ERR_CLI_USAGE_GET;
    }

    *pin = find_pin(pin_name);
    if (!*pin)
    {
        return E2S_ERR_CLI_UNKNOWN_PIN;
    }
    return E2S_OK;
}


e2s_error_t cli_parse( const char *line, char *cmd, char *args ){

    if (line[0] == '\0') return E2S_ERR_CLI_EMPTY_LINE;

    // Parse command and arguments
    int n = sscanf(line, "%15s", cmd);
    if (n == 1)
    {
        // Find start of arguments
        args[0] = '\0';
        const char *first_space = strchr(line, ' ');
        if (first_space)
        {
            const char *arg_start = first_space + 1;
            while (*arg_start == ' ') arg_start++;
            strcpy(args, arg_start);
        }
    }
    return E2S_OK;
}
