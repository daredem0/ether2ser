#include <stdarg.h>
#include <stdio.h>

#include "system/common.h"

typedef struct
{
    log_level_t current_log_level;
    bool        log_emitted;
    uint32_t    dropped_count;
    uint32_t    high_water_mark;
} mock_log_state_t;

static mock_log_state_t mock_log = {
    .current_log_level = LOG_LEVEL_INFO,
    .log_emitted       = false,
    .dropped_count     = 0U,
    .high_water_mark   = 0U,
};

void set_loglevel(log_level_t level)
{
    mock_log.current_log_level = level;
}

log_level_t get_loglevel(void)
{
    return mock_log.current_log_level;
}

void log_write(log_level_t level, const char* fmt, ...)
{
    if ((fmt == NULL) || (mock_log.current_log_level < level))
    {
        return;
    }

    // Keep format checking behavior in tests without writing to stdout.
    char    discard[1];
    va_list args;
    va_start(args, fmt);
    (void)vsnprintf(discard, sizeof(discard), fmt, args);
    va_end(args);

    if (level > LOG_LEVEL_PLAIN)
    {
        mock_log.log_emitted = true;
    }
}

void log_core1_drain(void)
{
    // No background drain needed in host-side unit tests.
}

uint32_t log_take_dropped_count(void)
{
    uint32_t dropped         = mock_log.dropped_count;
    mock_log.dropped_count   = 0U;
    return dropped;
}

uint32_t log_get_high_water_mark(void)
{
    return mock_log.high_water_mark;
}

bool log_take_emitted_flag(void)
{
    bool emitted          = mock_log.log_emitted;
    mock_log.log_emitted  = false;
    return emitted;
}
