#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "hardware/sync.h" // __dmb, __sev

#define LOG_QUEUE_DEPTH 128U
#define LOG_QUEUE_MASK (LOG_QUEUE_DEPTH - 1U)
#define LOG_LINE_MAX 160U

static inline void log_wake_core1(void)
{
#if defined(__arm__) || defined(__thumb__)
#if defined(__clang__) && defined(__has_builtin)
#if __has_builtin(__builtin_arm_sev)
    __builtin_arm_sev();
#else
    __asm volatile("sev" ::: "memory");
#endif
#else
    __asm volatile("sev" ::: "memory");
#endif
#elif defined(__riscv)
    __sev();
#else
    // Host/unit-test build: no-op.
#endif
}

#if (LOG_QUEUE_DEPTH & LOG_QUEUE_MASK) != 0
#error "LOG_QUEUE_DEPTH must be power of two"
#endif

typedef struct
{
    char line[LOG_LINE_MAX];
} log_entry_t;

typedef struct
{
    log_level_t       current_log_level;
    volatile uint16_t head; // producer writes, consumer reads
    volatile uint16_t tail; // consumer writes, producer reads
    volatile uint32_t dropped;
    volatile uint32_t high_water_mark;
    bool              log_emitted;
    log_entry_t       queue[LOG_QUEUE_DEPTH];
} global_log_state_t;

static global_log_state_t global_logstate = {.current_log_level = LOG_LEVEL_INFO,
                                             .head              = 0U,
                                             .tail              = 0U,
                                             .dropped           = 0U,
                                             .high_water_mark   = 0U,
                                             .log_emitted       = false};

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

    char    line[LOG_LINE_MAX];
    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    if (written < 0)
    {
        return;
    }
    line[LOG_LINE_MAX - 1U] = '\0';

    uint16_t head = global_logstate.head;
    uint16_t next = (uint16_t)((head + 1U) & LOG_QUEUE_MASK);

    if (next == global_logstate.tail)
    {
        global_logstate.dropped++;
        return;
    }

    memcpy(global_logstate.queue[head].line, line, LOG_LINE_MAX);
    __dmb(); // publish data before index update
    global_logstate.head = next;
    uint16_t depth       = (uint16_t)((next - global_logstate.tail) & LOG_QUEUE_MASK);
    if ((uint32_t)depth > global_logstate.high_water_mark)
    {
        global_logstate.high_water_mark = depth;
    }
    if (level > LOG_LEVEL_PLAIN)
    {
        global_logstate.log_emitted = true;
    }
    // wake core1 if sleeping
    log_wake_core1();
}

void log_core1_drain(void)
{
    bool wrote = false;
    while (global_logstate.tail != global_logstate.head)
    {
        uint16_t tail = global_logstate.tail;
        __dmb();
        (void)fputs(global_logstate.queue[tail].line, stdout);
        global_logstate.tail = (uint16_t)((tail + 1U) & LOG_QUEUE_MASK);
        wrote                = true;
    }
    if (wrote)
    {
        (void)fflush(stdout);
    }
}

uint32_t log_take_dropped_count(void)
{
    uint32_t dropped        = global_logstate.dropped;
    global_logstate.dropped = 0U;
    return dropped;
}

uint32_t log_get_high_water_mark(void)
{
    return global_logstate.high_water_mark;
}

bool log_take_emitted_flag(void)
{
    bool flag                   = global_logstate.log_emitted;
    global_logstate.log_emitted = false;
    return flag;
}
