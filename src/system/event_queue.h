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
#include <stdint.h>

// Project Headers
#include "drivers/pio_tx_rx_driver.h"

// Generated headers
#define EVENT_QUEUE_CAPACITY 16

typedef enum
{
    EV_NONE = 0,
    EV_CLI_LINE,
    EV_UDP_RX,
    EV_UDP_TX,
    EV_HDLC_DECODE,
    EV_SAVE_CONFIG,
    EV_WIPE_CONFIG,
    EV_SET_NET_SETTINGS,
    EV_GET_NET_SETTINGS,
    EV_SET_V24_SETTINGS,
    EV_GET_V24_SETTINGS
} event_type_t;

typedef enum
{
    NET_IP_REMOTE,
    NET_IP_GATEWAY,
    NET_IP_LOCAL,
    NET_IP_MASK,
    NET_PORT_LOCAL,
    NET_PORT_REMOTE,
    V24_BAUDRATE,
    V24_POLARITIES
} event_queue_data_types_t;

typedef struct
{
    event_queue_data_types_t id;
    union
    {
        uint8_t          ip[4];
        uint16_t         port;
        V24_POLARITIES_T polarities;
        V24_BAUDRATE_T   baudrate;
    } value;
} event_queue_data_t;

typedef struct
{
    event_type_t type;
    size_t       data_len;
    union
    {
        const void* ptr;       // for large/external data
        uint8_t     bytes[16]; // for small inline data
    } data;
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

#endif /* SYSTEM_EVENT_QUEUE_H */
