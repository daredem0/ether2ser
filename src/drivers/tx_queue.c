
/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/drivers/tx_queue.c
 * Purpose: TX queue storage and HDLC frame enqueue/dequeue.
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "tx_queue.h"

// Standard library headers
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Library Headers
#include "hardware/pio.h"
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"

// Project Headers
#include "drivers/pio_tx_rx_driver.h"
#include "drivers/w5500_driver.h"
#include "protocol/hdlc_encoder.h"
#include "system/common.h"
#include "system/error.h"
#include "system/ringbuffer.h"

// Generated headers

e2s_error_t tx_queue_init(TX_QUEUE_T* queue, uint8_t* buffer_data)
{
    RbInit(&(queue->queue_buffer), buffer_data, TX_FRAME_QUEUE_SIZE, sizeof(TX_QUEUE_ENTRY_T));
    queue->queue_touched = false;
    queue->tx_wire_bytes = 0;
    return E2S_OK;
}

#include "pico/time.h"
e2s_error_t poll_queue_stats(TX_QUEUE_T* queue)
{
    if (!queue)
    {
        return E2S_ERR_TX_QUEUE_NOT_INITIALIZED;
    }
    static bool     timer_init    = false;
    static uint32_t last_print_ms = 0;
    uint32_t        now_ms        = to_ms_since_boot(get_absolute_time());
    if (!timer_init)
    {
        timer_init    = true;
        last_print_ms = now_ms;
    }
    if (queue->queue_touched || (uint32_t)(now_ms - last_print_ms) >= 15000u)
    {
        LOG_DEBUG("TX Queue Stats: %zu / %zu frames used\r\n", queue->queue_buffer.count,
                  queue->queue_buffer.capacity);
        queue->queue_touched = false;
        last_print_ms        = now_ms;
    }
    return E2S_OK;
}
static inline TX_QUEUE_ENTRY_T tx_queue_entry_init(void)
{
    TX_QUEUE_ENTRY_T e = {0};
    e.frame.payload    = e.payload;
    e.frame.capacity   = sizeof(e.payload);
    return e;
}

static e2s_error_t tx_queue_drain_bytes(TX_QUEUE_T* queue, TX_QUEUE_ENTRY_T* entry,
                                        size_t bytes_to_drain)
{
    // LOG_DEBUG("TX: Draining up to %zu bytes from entry (length: %zu, offset: %zu)\r\n",
    //        bytes_to_drain, entry->frame.length, entry->offset);
    if (!entry)
    {
        // LOG_DEBUG("TX: Entry is NULL\r\n");
        return E2S_ERR_TX_QUEUE_NOT_INITIALIZED;
    }
    size_t effective_bytes_to_drain = bytes_to_drain >= entry->frame.length - entry->offset
                                          ? entry->frame.length - entry->offset
                                          : bytes_to_drain;
    size_t bytes_drained            = 0;
    for (size_t i = 0; i < effective_bytes_to_drain; i++)
    {
        if (pio_sm_is_tx_fifo_full(pio0, 0))
        {
            // LOG_DEBUG("TX: TX FIFO is full, stopping drain\r\n");
            // break;
        }
        if (tx_put(entry->frame.payload[entry->offset + i]))
        {
            // LOG_DEBUG("TX: Wrote byte %02X\r\n", entry->frame.payload[entry->offset + i]);
            bytes_drained++;
        }
        else
        {
            // LOG_DEBUG("TX: Failed to write byte %02X\r\n", entry->frame.payload[entry->offset +
            // i]);
            break;
        }
    }
    entry->offset += bytes_drained;
    queue->tx_wire_bytes += bytes_drained;
    // LOG_DEBUG("TX: Drained %zu bytes, new offset is %zu\r\n", bytes_drained, entry->offset);
    return E2S_OK;
}
size_t tx_queue_get_count(const TX_QUEUE_T* queue)
{
    if (!queue)
    {
        return 0;
    }
    return queue->queue_buffer.count;
}

bool tx_queue_is_empty(TX_QUEUE_T* queue)
{
    // Queue is empty only if ring buffer is empty AND current entry is fully drained
    return queue->queue_buffer.count == 0 &&
           queue->current_entry.offset >= queue->current_entry.frame.length;
}

e2s_error_t tx_queue_drain(TX_QUEUE_T* queue, size_t bytes_to_drain)
{
    if (!queue)
    {
        return E2S_ERR_TX_QUEUE_NOT_INITIALIZED;
    }
    // if (pio_sm_is_tx_fifo_full(pio0, 0))
    // {
    //     return E2S_OK; // TX FIFO is full, cannot drain now
    // }
    // queue->current_entry = (TX_QUEUE_ENTRY_T){0};
    if (queue->current_entry.offset >= queue->current_entry.frame.length)
    {
        size_t completed_offset = queue->current_entry.offset;
        size_t completed_length = queue->current_entry.frame.length;

        if (RbPopFront(&(queue->queue_buffer), &queue->current_entry) < 0)
        {
            return E2S_ERR_TX_QUEUE_NOT_INITIALIZED;
        }
        printf("TX FRAME COMPLETE: offset=%zu length=%zu\n", completed_offset, completed_length);

        queue->current_entry.frame.payload  = queue->current_entry.payload;
        queue->current_entry.frame.capacity = sizeof(queue->current_entry.payload);
    }
    tx_queue_drain_bytes(queue, &queue->current_entry, bytes_to_drain);
    if (queue->current_entry.offset > 0 &&
        queue->current_entry.offset < queue->current_entry.frame.length)
    {
        static uint32_t last_log = 0;
        uint32_t        now      = to_ms_since_boot(get_absolute_time());
        if (now - last_log > 1000)
        { // Log once per second
            printf("TX IN PROGRESS: offset=%zu/%zu\n", queue->current_entry.offset,
                   queue->current_entry.frame.length);
            last_log = now;
        }
    }
    return E2S_OK;
}

e2s_error_t tx_queue_enqueue_udp_frame(TX_QUEUE_T* queue, UDP_FRAME_T* frame)
{
    if (!queue || !frame)
    {
        return E2S_ERR_TX_QUEUE_NOT_INITIALIZED;
    }
    if (queue->queue_buffer.count == queue->queue_buffer.capacity)
    {
        return E2S_ERR_TX_QUEUE_FULL;
    }
    TX_QUEUE_ENTRY_T tx_entry = tx_queue_entry_init();
    tx_entry.frame.payload    = tx_entry.payload; // Fix pointer to point to OUR payload

    if (!hdlc_encode(frame->payload, frame->length, &tx_entry.frame, true))
    {
        return E2S_ERR_HDLC_ENCODE_FAILED;
    }
    LOG_DEBUG("TX: Enqueued %zu bytes\r\n", tx_entry.frame.length);
    if (RbPushBack(&(queue->queue_buffer), &tx_entry) < 0)
    {
        return E2S_ERR_TX_QUEUE_NOT_INITIALIZED;
    }
    queue->queue_touched = true;
    return E2S_OK;
}
