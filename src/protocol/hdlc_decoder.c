
#include "hdlc_decoder.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "hdlc_common.h"


bool hdlc_decode(const HDLC_FRAME_T *frame, uint8_t *payload, const size_t out_capacity, size_t *payload_length){
    if (frame == NULL || payload == NULL || payload_length == NULL || out_capacity == 0 ||
        frame->payload[0] != HDLC_FLAG_BYTE ||
        frame->payload[frame->length - 1] != HDLC_FLAG_BYTE){
        printf("Invalid frame\r\n");
        goto abort;
    }

    for (size_t i = 0; i < frame->length; i++){
        printf("0x%02X ", frame->payload[i]);
    }
    printf("\r\n");
    // First define the length with the assumption that no crc was escaped
    bool found_escape = false;
    size_t outbyte_ctr = 0;
    uint16_t crc16 = 0;
    uint8_t crc_marker = 8;
    printf("Frame length: %ld\r\n", frame->length);
    for (size_t frame_cntr = 1; frame_cntr < frame->length - 1; frame_cntr++){
        printf("Current byte: 0x%02X\r\n", frame->payload[frame_cntr]);
        printf("Loop Count: %ld\r\n", frame_cntr);
        if (frame->payload[frame_cntr] == HDLC_ESCAPE_BYTE){
            printf("Escaped byte found\r\n");
            found_escape = true;
            continue;
        }
        if (outbyte_ctr < out_capacity) {
            payload[outbyte_ctr++] = found_escape ? frame->payload[frame_cntr]^HDLC_ESCAPE_XOR : frame->payload[frame_cntr];
            printf("Wrote payload[%ld] = 0x%02X\r\n", outbyte_ctr - 1, payload[outbyte_ctr - 1]);
            found_escape = false;
        }
        else{
            printf("Outbyte_ctr: %ld, out_capacity: %ld\r\n", outbyte_ctr, out_capacity);
            if (outbyte_ctr > out_capacity) {
                printf("Payload too long\r\n");
                goto abort;
            }
        }
    }
    crc16 = payload[outbyte_ctr - 2] << 8 | payload[outbyte_ctr-1];
    *payload_length = (outbyte_ctr - 2);
    printf("Payload length: %ld\r\n", *payload_length);

    for (size_t i = 0; i < *payload_length && i < 5; i++){
        printf("0x%02X ", payload[i]);
    }
    printf("\r\n");

    // Check crc
    uint16_t recovered_crc = hdlc_crc16(payload, *payload_length);
    if (crc16 != recovered_crc){
        printf("CRC check failed, got 0x%04X expected 0x%04X\r\n", recovered_crc, crc16);
        goto abort;
    }


    return true;

abort:
    if (payload_length){*payload_length = 0;}
    return false;
}
