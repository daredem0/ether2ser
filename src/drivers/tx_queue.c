
/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/drivers/tx_queue.c
 * Purpose: TX queue storage and HDLC frame enqueue/dequeue.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "tx_queue.h"

// Standard library headers
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// Project Headers
#include "drivers/pio_tx_rx_driver.h"
#include "drivers/w5500_driver.h"
#include "protocol/hdlc_encoder.h"
#include "system/common.h"
#include "system/error.h"
#include "system/ringbuffer.h"

// Generated headers

#define TX_QUEUE_TX_INPROG_MS 1000U

e2s_error_t tx_queue_init(TX_QUEUE_T* queue, uint8_t* buffer_data)
{
    memset(&queue->current_entry, 0, sizeof(queue->current_entry));
    RbInit(&(queue->queue_buffer), buffer_data, TX_FRAME_QUEUE_SIZE, sizeof(TX_QUEUE_ENTRY_T));
    queue->queue_touched = false;
    queue->tx_wire_bytes = 0;
    return E2S_OK;
}

#include "pico/time.h"
e2s_error_t poll_queue_stats(TX_QUEUE_T* queue)
{
#define QUEUE_FILLING_UP_THRESHOLD 5U
    if (!queue)
    {
        return E2S_ERR_TX_QUEUE_NOT_INITIALIZED;
    }
    bool queue_filling_up =
        (queue->queue_buffer.capacity - queue->queue_buffer.count < QUEUE_FILLING_UP_THRESHOLD);
    if (queue->queue_touched || queue_filling_up)
    {
        if (queue_filling_up)
        {
            LOG_TRACE("TX Queue Stats: %zu / %zu frames used\r\n", queue->queue_buffer.count,
                      queue->queue_buffer.capacity);
        }
        else
        {
            LOG_INFO("TX Queue Stats: %zu / %zu frames used\r\n", queue->queue_buffer.count,
                     queue->queue_buffer.capacity);
        }
        queue->queue_touched = false;
    }
    return E2S_OK;
}

static e2s_error_t tx_queue_drain_bytes(TX_QUEUE_T* queue, TX_QUEUE_ENTRY_T* entry,
                                        size_t bytes_to_drain, size_t* bytes_drained)
{
    // LOG_DEBUG("TX: Draining up to %zu bytes from entry (length: %zu, offset: %zu)\r\n",
    //        bytes_to_drain, entry->frame.length, entry->offset);
    if (!entry || !bytes_drained)
    {
        // LOG_DEBUG("TX: Entry is NULL\r\n");
        return E2S_ERR_TX_QUEUE_NOT_INITIALIZED;
    }
    size_t effective_bytes_to_drain = bytes_to_drain >= entry->frame.length - entry->offset
                                          ? entry->frame.length - entry->offset
                                          : bytes_to_drain;
    *bytes_drained                  = 0;
    for (size_t i = 0; i < effective_bytes_to_drain; i++)
    {
        if (v24_tx_try_write(entry->frame.payload[entry->offset + i]))
        {
            // LOG_DEBUG("TX: Wrote byte %02X\r\n", entry->frame.payload[entry->offset + i]);
            (*bytes_drained)++;
        }
        else
        {
            // LOG_DEBUG("TX: Failed to write byte %02X\r\n", entry->frame.payload[entry->offset +
            // i]);
            break;
        }
    }
    entry->offset += *bytes_drained;
    queue->tx_wire_bytes += *bytes_drained;
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
    return queue->queue_buffer.count == 0U && queue->current_entry.frame.length == 0U;
}

bool tx_queue_has_active_frame(const TX_QUEUE_T* queue)
{
    return queue != NULL && queue->current_entry.frame.length != 0U;
}

e2s_error_t tx_queue_drain(TX_QUEUE_T* queue, size_t bytes_to_drain, size_t* bytes_drained)
{
    if (!queue || !bytes_drained)
    {
        return E2S_ERR_TX_QUEUE_NOT_INITIALIZED;
    }
    *bytes_drained = 0U;

    v24_tx_event_t tx_event = v24_tx_poll();
    if (tx_event == V24_TX_EVENT_RESTART_FRAME &&
        queue->current_entry.frame.length != 0U)
    {
        queue->current_entry.offset = 0U;
        return E2S_OK;
    }
    if (tx_event == V24_TX_EVENT_FRAME_COMPLETE &&
        queue->current_entry.frame.length != 0U)
    {
        LOG_DEBUG("TX FRAME COMPLETE: offset=%zu length=%zu\n",
                  queue->current_entry.offset, queue->current_entry.frame.length);
        queue->current_entry.frame.length = 0U;
        queue->current_entry.offset       = 0U;
        return E2S_OK;
    }

    bool frame_active = queue->current_entry.frame.length != 0U;
    if (frame_active &&
        queue->current_entry.offset >= queue->current_entry.frame.length)
    {
        return E2S_OK;
    }

    if (!frame_active)
    {
        if (RbPopFront(&(queue->queue_buffer), &queue->current_entry) < 0)
        {
            return E2S_OK;
        }
        queue->current_entry.frame.payload  = queue->current_entry.payload;
        queue->current_entry.frame.capacity = sizeof(queue->current_entry.payload);
    }

    size_t offset_before_drain = queue->current_entry.offset;
    tx_queue_drain_bytes(queue, &queue->current_entry, bytes_to_drain, bytes_drained);
    if (queue->current_entry.offset >= queue->current_entry.frame.length &&
        offset_before_drain < queue->current_entry.frame.length)
    {
        v24_tx_mark_frame_queued();
    }
    if (queue->current_entry.offset > 0U &&
        queue->current_entry.offset < queue->current_entry.frame.length)
    {
        static uint32_t last_log = 0U;
        uint32_t        now      = to_ms_since_boot(get_absolute_time());
        if (now - last_log > TX_QUEUE_TX_INPROG_MS)
        {
            LOG_TRACE("TX IN PROGRESS: offset=%zu/%zu\r\n", queue->current_entry.offset,
                      queue->current_entry.frame.length);
            last_log = now;
        }
    }
    return E2S_OK;
}


e2s_error_t tx_queue_enqueue_udp_frame(TX_QUEUE_T* queue, const UDP_FRAME_T* frame)
{
    if (!queue || !frame)
    {
        return E2S_ERR_TX_QUEUE_NOT_INITIALIZED;
    }
    if (frame->length > UDP_MAX_PAYLOAD_BYTES)
    {
        return E2S_ERR_TX_QUEUE_PAYLOAD_TOO_LARGE;
    }
    if (queue->queue_buffer.count == queue->queue_buffer.capacity)
    {
        return E2S_ERR_TX_QUEUE_FULL;
    }
    TX_QUEUE_ENTRY_T tx_entry = {0};
    tx_entry.frame.payload    = tx_entry.payload;
    tx_entry.frame.capacity   = sizeof(tx_entry.payload);
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
