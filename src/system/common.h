
/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/common.h
 * Purpose: Shared helpers and logging macros.
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef COMMON_H
#define COMMON_H

// Related headers

// Standard library headers
#include <stddef.h>
#include <stdio.h>

// Project Headers

// Generated headers

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

typedef enum { LOG_LEVEL_INFO, LOG_LEVEL_DEBUG } log_level_t;
void set_loglevel(log_level_t level);
extern log_level_t current_log_level;

#define LOG_DEBUG(...) do { if (current_log_level >= LOG_LEVEL_DEBUG) printf(__VA_ARGS__); } while(0)
#define LOG_INFO(...)  do { if (current_log_level >= LOG_LEVEL_INFO) printf(__VA_ARGS__); } while(0)

#define PRINT_FRAME_HEX(label, payload_ptr, length)                                  \
    do {                                                                            \
        LOG_DEBUG("%s", (label));                                                      \
        for (size_t _i = 0; _i < (length); _i++) {                                  \
            LOG_DEBUG("%02X ", (unsigned)(payload_ptr)[_i]);                           \
            if (_i % 16 == 15) {                                                    \
                LOG_DEBUG("\r\n");                                                     \
            }                                                                       \
        }                                                                           \
        LOG_DEBUG("\r\n");                                                             \
    } while (0)

#endif /* COMMON_H */
