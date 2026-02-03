/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/log.c
 * Purpose: Log level state and configuration.
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "common.h"

// Standard library headers

// Project Headers

// Generated headers

log_level_t current_log_level = LOG_LEVEL_INFO;

void set_loglevel(log_level_t level)
{
    current_log_level = level;
}
