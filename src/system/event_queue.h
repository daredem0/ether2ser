/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/event_queue.h
 * Purpose: Event queue API with opaque payload pointers.
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef SYSTEM_EVENT_QUEUE_H
#define SYSTEM_EVENT_QUEUE_H

// Related headers

// Standard library headers
#include <stdbool.h>
#include <stddef.h>

// Library Headers

// Project Headers

// Generated headers
#define EVENT_QUEUE_CAPACITY 16

typedef enum
{
    EV_NONE = 0,
    EV_CLI_LINE,
    EV_UDP_RX,
} event_type_t;

typedef struct
{
    event_type_t type;
    const void *data; // opaque payload pointer owned by caller
    size_t data_len;  // length of payload in bytes
} event_t;

/**
 * @brief Initialize the event queue storage.
 */
void event_queue_init(void);

/**
 * @brief Enqueue an event.
 * @param event_entry Event to push; payload pointer ownership stays with caller.
 * @return true if queued, false if the queue was full.
 */
bool event_queue_push(const event_t *event_entry);

/**
 * @brief Dequeue an event.
 * @param event_out Destination for the popped event.
 * @return true if an event was popped, false if the queue was empty.
 */
bool event_queue_pop(event_t *event_out);

/**
 * @brief Check whether the queue is empty.
 */
bool event_queue_is_empty(void);

/**
 * @brief Check whether the queue is full.
 */
bool event_queue_is_full(void);

#endif /* SYSTEM_EVENT_QUEUE_H */
