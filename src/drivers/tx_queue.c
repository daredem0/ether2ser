





// Related headers
#include "tx_queue.h"

// Standard library headers
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

// Library Headers
#include "pico/stdio.h"
#include "pico/time.h"
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"

// Project Headers
#include "system/cli_commands.h"
#include "system/event_queue.h"
#include "system/cli_usb_cdc.h"
#include "system/baudrate_monitor.h"
#include "drivers/w5500_driver.h"
#include "drivers/pio_tx_rx_driver.h"
#include "platform/pinmap.h"
#include "system/ringbuffer.h"
#include "protocol/hdlc_encoder.h"

// Generated headers

e2s_error_t tx_queue_init(TX_QUEUE_T *queue, uint8_t *buffer_data){
    RbInit(&(queue->queue_buffer), buffer_data, TX_FRAME_QUEUE_SIZE, sizeof(TX_QUEUE_ENTRY_T));
    queue->queue_touched = false;
    return E2S_OK;
}

e2s_error_t poll_queue_stats(TX_QUEUE_T *queue){
    if (!queue) {
        return E2S_ERR_TX_QUEUE_NOT_INITIALIZED;
    }
    if (queue->queue_touched){
        printf("TX Queue Stats: %zu / %zu frames used\r\n",
            queue->queue_buffer.count,
            queue->queue_buffer.capacity);
        queue->queue_touched = false;
    }
    return E2S_OK;
}
static inline TX_QUEUE_ENTRY_T tx_queue_entry_init(void) {
    TX_QUEUE_ENTRY_T e = {0};
    e.frame.payload = e.payload;
    e.frame.capacity = sizeof(e.payload);
    return e;
}

static e2s_error_t tx_queue_drain_bytes(TX_QUEUE_ENTRY_T *entry, size_t bytes_to_drain){
    // printf("TX: Draining up to %zu bytes from entry (length: %zu, offset: %zu)\r\n",
    //        bytes_to_drain, entry->frame.length, entry->offset);
    if (!entry){
        // printf("TX: Entry is NULL\r\n");
        return E2S_ERR_TX_QUEUE_NOT_INITIALIZED;
    }
    size_t effective_bytes_to_drain = bytes_to_drain >= entry->frame.length - entry->offset ?
                                      entry->frame.length - entry->offset :
                                      bytes_to_drain;
    size_t bytes_drained = 0;
    for (size_t i = 0; i < effective_bytes_to_drain; i++){
        if(pio_sm_is_tx_fifo_full(pio0, 0)){
            // printf("TX: TX FIFO is full, stopping drain\r\n");
            break;
        }
        if (tx_put(entry->frame.payload[entry->offset + i])){
            // printf("TX: Wrote byte %02X\r\n", entry->frame.payload[entry->offset + i]);
            bytes_drained++;
        } else {
            // printf("TX: Failed to write byte %02X\r\n", entry->frame.payload[entry->offset + i]);
            break;
        }
    }
    entry->offset += bytes_drained;
    // printf("TX: Drained %zu bytes, new offset is %zu\r\n", bytes_drained, entry->offset);
    return E2S_OK;
}

bool tx_queue_is_empty(TX_QUEUE_T *queue){
    return queue->queue_buffer.count == 0;
}

e2s_error_t tx_queue_drain(TX_QUEUE_T *queue, size_t bytes_to_drain){
    if (!queue){
        return E2S_ERR_TX_QUEUE_NOT_INITIALIZED;
    }
    if(pio_sm_is_tx_fifo_full(pio0, 0)){
        return E2S_OK; // TX FIFO is full, cannot drain now
    }
    static TX_QUEUE_ENTRY_T current_entry = {0};
    if(current_entry.offset == 0 || current_entry.offset >= current_entry.frame.length){
        if (RbPopFront(&(queue->queue_buffer), &current_entry) < 0){
            return E2S_ERR_TX_QUEUE_NOT_INITIALIZED;
        }
    }
    tx_queue_drain_bytes(&current_entry, bytes_to_drain);

    return E2S_OK;
}


e2s_error_t tx_queue_enqueue_udp_frame(TX_QUEUE_T *queue, UDP_FRAME_T *frame){
    if (!queue || !frame){
        return E2S_ERR_TX_QUEUE_NOT_INITIALIZED;
    }
    if (queue->queue_buffer.count == queue->queue_buffer.capacity){
        return E2S_ERR_TX_QUEUE_FULL;
    }
    TX_QUEUE_ENTRY_T tx_entry = tx_queue_entry_init();
    if (!hdlc_encode(frame->payload, frame->length, &tx_entry.frame)){
        return E2S_ERR_HDLC_ENCODE_FAILED;
    }
    if (RbPushBack(&(queue->queue_buffer), &tx_entry) < 0){
        return E2S_ERR_TX_QUEUE_NOT_INITIALIZED;
    }
    queue->queue_touched = true;
    return E2S_OK;
}
