
#include "hdlc_decoder.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "hdlc_common.h"


bool hdlc_decode(const HDLC_FRAME_T *frame, uint8_t *payload, const size_t out_capacity, size_t *payload_length){
    if (frame->payload[0] != HDLC_FLAG_BYTE || frame->payload[frame->length - 1] != HDLC_FLAG_BYTE){
        goto abort;
    }

    return true;

abort:
    return false;
}
