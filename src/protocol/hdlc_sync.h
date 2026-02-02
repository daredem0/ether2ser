


#ifndef HDLC_SYNC_H
#define HDLC_SYNC_H

#include "hdlc_common.h"
#include "system/error.h"
#include <stdbool.h>

#define RX_HDLC_SYNC_MAX_BUFFER_SIZE 2048
#define HDLC_SYNC_DEFAULT_SYNC_BYTE HDLC_FLAG_BYTE
typedef enum {
    HDLC_SYNC_STATE_HUNTING,
    HDLC_SYNC_STATE_SYNCING,
    HDLC_SYNC_STATE_SYNCED,
} HDLC_SYNC_STATE_T;

typedef struct {
    uint8_t buffer[RX_HDLC_SYNC_MAX_BUFFER_SIZE];
    size_t position;
    uint8_t bit_offset;
    HDLC_SYNC_STATE_T state;
    uint8_t sync_byte;
    uint16_t sync_accumulator;
} HDLC_SYNC_ACCUMULATOR_T;

void hdlc_sync_acc_init(HDLC_SYNC_ACCUMULATOR_T *accumulator, uint8_t sync_byte);
bool hdlc_sync_acc_process_byte(HDLC_SYNC_ACCUMULATOR_T *accumulator, uint8_t byte);
e2s_error_t hdlc_sync_acc_poll(HDLC_SYNC_ACCUMULATOR_T *accumulator, HDLC_FRAME_T *out_frame);

#endif /* HDLC_SYNC_H */
