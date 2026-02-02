

#ifndef TX_QUEUE_H
#define TX_QUEUE_H

#include "drivers/w5500_driver.h"
#include "system/ringbuffer.h"
#include "system/error.h"
#include "protocol/hdlc_common.h"
#include <stdbool.h>

#define TX_FRAME_QUEUE_SIZE 4

#define TX_QUEUE_DECLARE_AND_INIT(var_name) \
    TX_QUEUE_T var_name; \
    uint8_t var_name##_buffer_data[TX_FRAME_QUEUE_SIZE * sizeof(HDLC_FRAME_T)]; \
    Ringbuffer var_name##_ringbuf; \
    RbInit(&var_name##_ringbuf, var_name##_buffer_data, TX_FRAME_QUEUE_SIZE, sizeof(HDLC_FRAME_T)); \
    tq_queue_init(&var_name, &var_name##_ringbuf)

typedef struct {
    Ringbuffer *queue_buffer;
    bool queue_touched;
} TX_QUEUE_T;

typedef struct {
    uint8_t payload[1500];
    HDLC_FRAME_T frame;
    size_t offset;
} TX_QUEUE_ENTRY_T;

e2s_error_t tx_queue_enqueue_udp_frame(TX_QUEUE_T *queue, UDP_FRAME_T *frame);
bool tx_queue_is_empty(TX_QUEUE_T *queue);
e2s_error_t tx_queue_drain(TX_QUEUE_T *queue, size_t bytes_to_drain);
e2s_error_t poll_queue_stats(TX_QUEUE_T *queue);
e2s_error_t tx_queue_init(TX_QUEUE_T *queue, Ringbuffer *buffer);

#endif /* TX_QUEUE_H */
