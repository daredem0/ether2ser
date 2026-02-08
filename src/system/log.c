/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/log.c
 * Purpose: Log level state and configuration.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "common.h"

// Standard library headers
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

// Project Headers

// Generated headers
typedef struct
{
    log_level_t current_log_level;
    bool        log_emitted;
} global_log_state_t;

global_log_state_t global_logstate = {.current_log_level = LOG_LEVEL_INFO, .log_emitted = false};

log_level_t get_loglevel(void)
{
    return global_logstate.current_log_level;
}

void set_loglevel(log_level_t level)
{
    global_logstate.current_log_level = level;
}

void log_write(log_level_t level, const char* fmt, ...)
{
    if (global_logstate.current_log_level < level)
    {
        return;
    }

    global_logstate.log_emitted = true;
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

bool log_take_emitted_flag(void)
{
    bool flag                   = global_logstate.log_emitted;
    global_logstate.log_emitted = false;
    return flag;
}
