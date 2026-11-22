/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/event_queue.h
 * Purpose: Event queue API with opaque payload pointers.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef SYSTEM_EVENT_QUEUE_H
#define SYSTEM_EVENT_QUEUE_H

// Related headers

// Standard library headers
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Project Headers

// Generated headers
/**
 * @brief Maximum number of event slots in queue storage.
 */
#define EVENT_QUEUE_CAPACITY 16

/**
 * @brief Event type identifiers.
 */
typedef enum
{
    EV_NONE = 0,
    EV_CLI_LINE,
    EV_SAVE_CONFIG,
    EV_WIPE_CONFIG,
    EV_REBOOT
} event_type_t;


/**
 * @brief Event queue entry.
 */
typedef struct
{
    /** Event type. */
    event_type_t type;
    /** Number of valid bytes in payload storage. */
    size_t data_len;
    /** Payload storage union. */
    union
    {
        const void* ptr;       // for large/external data
        uint8_t     bytes[16]; // for small inline data
    } data;
    /** Selects inline payload (`true`) or pointer payload (`false`). */
    bool is_inline;
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
bool event_queue_push(const event_t* event_entry);

/**
 * @brief Dequeue an event.
 * @param event_out Destination for the popped event.
 * @return true if an event was popped, false if the queue was empty.
 */
bool event_queue_pop(event_t* event_out);

/**
 * @brief Check whether the queue is empty.
 */
bool event_queue_is_empty(void);

/**
 * @brief Check whether the queue is full.
 */
bool event_queue_is_full(void);

/**
 * @brief Get current number of queued events.
 * @return Queue fill count.
 */
size_t event_queue_get_count(void);

/**
 * @brief Get peak queue fill count observed since init.
 * @return High-water mark.
 */
size_t event_queue_get_high_water_mark(void);

/**
 * @brief Get cumulative number of dropped push attempts.
 * @return Drop counter.
 */
uint32_t event_queue_get_push_drop_count(void);

/**
 * @brief Reset queue high-water and drop statistics without removing events.
 */
void event_queue_reset_statistics(void);

/**
 * @brief Resolve payload pointer and validate minimum payload size.
 * @param event Event to inspect.
 * @param required_size Minimum bytes required by caller.
 * @param out Output pointer to payload bytes/object.
 * @return true when payload is valid and large enough.
 */
bool event_get_payload_ptr(const event_t* event, size_t required_size, const void** out);

#endif /* SYSTEM_EVENT_QUEUE_H */
