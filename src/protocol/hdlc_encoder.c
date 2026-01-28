

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "hdlc_common.h"
#include "hdlc_encoder.h"


#define HDLC_TRY_PUT_BYTE(byte, frame, error_handling) \
    do { \
        if((frame)->length + 1 > (frame)->capacity) { goto error_handling; } \
        (frame)->payload[(frame)->length++] = byte; \
    } while(0)


static bool hdlc_escape_if_needed(uint8_t byte, HDLC_FRAME_T *frame){
    if(byte == HDLC_FLAG_BYTE || byte == HDLC_ESCAPE_BYTE){
        HDLC_TRY_PUT_BYTE(HDLC_ESCAPE_BYTE, frame, abort);
        HDLC_TRY_PUT_BYTE(byte^HDLC_ESCAPE_XOR, frame, abort);
    }
    else{
        HDLC_TRY_PUT_BYTE(byte, frame, abort);
    }
    return true;
abort:
    return false;
}


bool hdlc_encode(const uint8_t *payload, const size_t payload_length, HDLC_FRAME_T *frame){
    if (frame == NULL || (payload == NULL && payload_length > 0) ||
        (frame->capacity < 2) || frame->length != 0){
        goto abort;
    }

    // Write opening flag
    HDLC_TRY_PUT_BYTE(HDLC_FLAG_BYTE, frame, abort);

    // Write data (only if there is actual data to write)
    for(size_t i = 0; i < payload_length; i++){
        if (!hdlc_escape_if_needed(payload[i], frame)) goto abort;
    }

    uint16_t crc16 = hdlc_crc16(payload, payload_length);
    if (!hdlc_escape_if_needed((crc16 >> 8) & 0xFF, frame)) goto abort;
    if (!hdlc_escape_if_needed(crc16 & 0xFF, frame)) goto abort;

    // Write closing flag
    HDLC_TRY_PUT_BYTE(HDLC_FLAG_BYTE, frame, abort);
    return true;
abort:
    printf("Warning: HDLC encode failed!\r\n");
    if (frame){frame->length = 0;}
    return false;
}
