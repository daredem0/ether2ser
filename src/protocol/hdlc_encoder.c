

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "hdlc_encoder.h"


#define HDLC_TRY_PUT_BYTE(byte, frame, error_handling) \
    do { \
        if((frame)->length + 1 > (frame)->capacity) { goto error_handling; } \
        (frame)->payload[(frame)->length++] = byte; \
    } while(0)

bool hdlc_encode(const uint8_t *payload, const size_t payload_length, HDLC_FRAME_T *frame){
    if (frame == NULL || (payload == NULL && payload_length > 0) ||
        (frame->capacity < 2) || frame->length != 0){
        goto abort;
    }

    // Write opening flag
    HDLC_TRY_PUT_BYTE(HDLC_FLAG_BYTE, frame, abort);

    // Write data (only if there is actual data to write)
    for(size_t i = 0; i < payload_length; i++){
        if(payload[i] == HDLC_FLAG_BYTE || payload[i] == HDLC_ESCAPE_BYTE){
            printf("Escape byte required!\r\n");
            HDLC_TRY_PUT_BYTE(HDLC_ESCAPE_BYTE, frame, abort);
            HDLC_TRY_PUT_BYTE(payload[i]^HDLC_ESCAPE_XOR, frame, abort);
        }
        else{
            HDLC_TRY_PUT_BYTE(payload[i], frame, abort);
        }
    }

    // Write closing flag
    HDLC_TRY_PUT_BYTE(HDLC_FLAG_BYTE, frame, abort);
    return true;
abort:
    printf("Warning: HDLC encode failed!\r\n");
    if (frame){frame->length = 0;}
    return false;
}
