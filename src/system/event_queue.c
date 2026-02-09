/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/event_queue.c
 * Purpose: Opaque event queue storage and operations.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "event_queue.h"

// Standard library headers
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Project Headers

// Generated headers

// Event queue storage (opaque payload pointers)
static event_t event_queue[EVENT_QUEUE_CAPACITY];
static uint8_t event_queue_write = 0;
static uint8_t event_queue_read  = 0;
static uint8_t event_queue_high_water_mark = 0;
static uint32_t event_queue_push_drops = 0;

static uint8_t event_queue_count_internal(void)
{
    if (event_queue_write >= event_queue_read)
    {
        return (uint8_t)(event_queue_write - event_queue_read);
    }
    return (uint8_t)(EVENT_QUEUE_CAPACITY - (event_queue_read - event_queue_write));
}

void event_queue_init(void)
{
    memset(event_queue, 0, sizeof(event_queue));
    event_queue_write = 0;
    event_queue_read  = 0;
    event_queue_high_water_mark = 0;
    event_queue_push_drops = 0;
}

bool event_queue_push(const event_t* event_entry)
{
    uint8_t next_write = (uint8_t)((event_queue_write + 1) % EVENT_QUEUE_CAPACITY);
    if (next_write == event_queue_read)
    {
        event_queue_push_drops++;
        return false; // full
    }
    event_queue[event_queue_write] = *event_entry;
    event_queue_write              = next_write;
    uint8_t current_count = event_queue_count_internal();
    if (current_count > event_queue_high_water_mark)
    {
        event_queue_high_water_mark = current_count;
    }
    return true;
}

bool event_queue_pop(event_t* event_out)
{
    if (event_queue_read == event_queue_write)
    {
        return false; // empty
    }
    *event_out       = event_queue[event_queue_read];
    event_queue_read = (uint8_t)((event_queue_read + 1) % EVENT_QUEUE_CAPACITY);
    return true;
}

bool event_queue_is_empty(void)
{
    return event_queue_read == event_queue_write;
}

bool event_queue_is_full(void)
{
    uint8_t next_write = (uint8_t)((event_queue_write + 1) % EVENT_QUEUE_CAPACITY);
    return next_write == event_queue_read;
}

size_t event_queue_get_count(void)
{
    return (size_t)event_queue_count_internal();
}

size_t event_queue_get_high_water_mark(void)
{
    return (size_t)event_queue_high_water_mark;
}

uint32_t event_queue_get_push_drop_count(void)
{
    return event_queue_push_drops;
}

bool event_get_payload_ptr(const event_t* event, size_t required_size, const void** out)
{
    if (!event || !out || required_size == 0)
    {
        return false;
    }

    if (event->is_inline)
    {
        if (event->data_len < required_size)
        {
            return false;
        }
        *out = (const void*)event->data.bytes;
        return true;
    }

    if (!event->data.ptr || event->data_len < required_size)
    {
        return false;
    }

    *out = event->data.ptr;
    return true;
}
