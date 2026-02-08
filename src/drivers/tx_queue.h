/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/drivers/tx_queue.h
 * Purpose: TX queue API for buffered HDLC frames.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef TX_QUEUE_H
#define TX_QUEUE_H

// Related headers

// Standard library headers
#include <stdbool.h>
#include <stdint.h>

// Project Headers
#include "drivers/w5500_driver.h"
#include "protocol/hdlc_common.h"
#include "system/error.h"
#include "system/ringbuffer.h"

// Generated headers

/**
 * @brief Number of frame entries available in the TX queue ring buffer.
 */
#define TX_FRAME_QUEUE_SIZE 32

/**
 * @brief Convenience macro to declare and initialize a TX queue and backing storage.
 * @param var_name Base variable name for queue and storage symbols.
 */
#define TX_QUEUE_DECLARE_AND_INIT(var_name)                                        \
    TX_QUEUE_T var_name;                                                           \
    uint8_t    var_name##_buffer_data[TX_FRAME_QUEUE_SIZE * sizeof(HDLC_FRAME_T)]; \
    Ringbuffer var_name##_ringbuf;                                                 \
    RbInit(&var_name##_ringbuf, var_name##_buffer_data, TX_FRAME_QUEUE_SIZE,       \
           sizeof(HDLC_FRAME_T));                                                  \
    tq_queue_init(&var_name, &var_name##_ringbuf)

/**
 * @brief One queued HDLC frame plus drain offset state.
 */
typedef struct
{
    /** Backing payload storage for the encoded frame. */
    uint8_t      payload[4000];
    /** Encoded frame descriptor. */
    HDLC_FRAME_T frame;
    /** Number of bytes already drained to TX FIFO. */
    size_t       offset;
} TX_QUEUE_ENTRY_T;

/**
 * @brief TX queue runtime state.
 */
typedef struct
{
    /** Currently active entry being drained to PIO. */
    TX_QUEUE_ENTRY_T current_entry;
    /** Ring buffer of pending entries. */
    Ringbuffer       queue_buffer;
    /** Flag used to trigger periodic queue stats printouts. */
    bool             queue_touched;
    /** Total bytes pushed to wire via TX FIFO. */
    uint64_t         tx_wire_bytes;
} TX_QUEUE_T;

/**
 * @brief Encode UDP frame to HDLC and append it to TX queue.
 * @param queue TX queue instance.
 * @param frame UDP frame payload source.
 * @return Error code.
 */
e2s_error_t tx_queue_enqueue_udp_frame(TX_QUEUE_T* queue, UDP_FRAME_T* frame);

/**
 * @brief Check whether queue and active entry are fully drained.
 * @param queue TX queue instance.
 * @return true if no pending or active bytes remain.
 */
bool        tx_queue_is_empty(TX_QUEUE_T* queue);

/**
 * @brief Drain up to @p bytes_to_drain bytes from queue into TX FIFO.
 * @param queue TX queue instance.
 * @param bytes_to_drain Maximum bytes to attempt.
 * @return Error code.
 */
e2s_error_t tx_queue_drain(TX_QUEUE_T* queue, size_t bytes_to_drain);

/**
 * @brief Emit queue usage statistics when needed.
 * @param queue TX queue instance.
 * @return Error code.
 */
e2s_error_t poll_queue_stats(TX_QUEUE_T* queue);

/**
 * @brief Initialize TX queue and ring buffer storage.
 * @param queue TX queue instance.
 * @param buffer_data Backing storage for ring buffer entries.
 * @return Error code.
 */
e2s_error_t tx_queue_init(TX_QUEUE_T* queue, uint8_t* buffer_data);

/**
 * @brief Get current number of queued entries.
 * @param queue TX queue instance.
 * @return Entry count.
 */
size_t      tx_queue_get_count(const TX_QUEUE_T* queue);

#endif /* TX_QUEUE_H */
