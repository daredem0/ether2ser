
/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/common.h
 * Purpose: Shared helpers and logging macros.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef COMMON_H
#define COMMON_H

// Related headers

// Standard library headers
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

// Project Headers

// Generated headers

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

#define RX_SHIFT_TO_LSB (3U * CHAR_BIT)

// Important: Keep this up with reality. It has to match the actual amount
// of cycles used in the pio program, otherwise the clock will be off
#define TX_PIO_CYCLES_PER_BIT 3U

/*
 * Some arm-none-eabi/newlib combinations may miss PRI* macros unless the
 * headers line up exactly. Provide conservative fallbacks when absent.
 */
#ifndef PRIu32
#define PRIu32 "u"
#endif

#ifndef PRIX32
#define PRIX32 "X"
#endif

#ifndef PRIu64
#define PRIu64 "llu"
#endif

typedef enum
{
    LOG_LEVEL_ERROR,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_TRACE
} log_level_t;
void        set_loglevel(log_level_t level);
log_level_t get_loglevel(void);
bool        log_take_emitted_flag(void);

// extern log_level_t current_log_level;

static inline const char* log_level_tag(log_level_t level)
{
    switch (level)
    {
    case LOG_LEVEL_ERROR:
        return "ERROR";
    case LOG_LEVEL_INFO:
        return "INFO";
    case LOG_LEVEL_DEBUG:
        return "DEBUG";
    case LOG_LEVEL_TRACE:
        return "TRACE";
    default:
        return "LOG";
    }
}
void log_write(log_level_t level, const char* fmt, ...);
#define LOG(level, fmt, ...)                      \
    do                                            \
    {                                             \
        log_write((level), (fmt), ##__VA_ARGS__); \
    } while (0)

#define LOG_ERROR(...) LOG(LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_INFO(...) LOG(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_DEBUG(...) LOG(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define LOG_TRACE(...) LOG(LOG_LEVEL_TRACE, __VA_ARGS__)

#define PRINT_FRAME_HEX(label, payload_ptr, length)          \
    do                                                       \
    {                                                        \
        LOG_DEBUG("%s", (label));                            \
        for (size_t _i = 0; _i < (length); _i++)             \
        {                                                    \
            LOG_DEBUG("%02X ", (unsigned)(payload_ptr)[_i]); \
            if (_i % 16 == 15)                               \
            {                                                \
                LOG_DEBUG("\r\n");                           \
            }                                                \
        }                                                    \
        LOG_DEBUG("\r\n");                                   \
    } while (0)

#endif /* COMMON_H */
