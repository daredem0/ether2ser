
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

    /* Check if crc contains escapes. Normally the crc would be end -1 and end -2.
    But these bytes could possibly be the results of xor operation. Worst case
    both crc values were escaped. So we have to check first if we found a escape
    in -1..-3 and if we did, we also have to check if we find another one in -4.
    Frame end with one two escaped crcs:
    0x7D 0x12 0x7D 0x12 0x7E
     -4   -3   -2   -1   end
    Frame with first crc escaped
    0x7D 0x12 0x23 0x7E
     -3   -2   -1   end
    Frame with second crc escaped
    0x12 0x7D 0x23 0x7E
     -3   -2   -1   end
    Frame with no crc escaped
    0x12 0x23 0x7E
     -2   -1   end
    */

    for (size_t i = 0; i < frame->length; i++){
        printf("0x%02X ", frame->payload[i]);
    }
    printf("\r\n");
    // First define the length with the assumption that no crc was escaped
    size_t out_length = frame->length - 2; // - 2 Flags - 2 crc
    bool found_escape = false;
    bool crc_range = false;
    size_t outbyte_ctr = 0;
    uint16_t crc16 = 0;
    uint8_t crc_marker = 0;
    printf("Frame length: %ld\r\n", frame->length);
    for (size_t frame_cntr = 1; frame_cntr < frame->length - 1; frame_cntr++){
        printf("Loop Count: %ld\r\n", frame_cntr);
        if (frame->payload[frame_cntr] == HDLC_ESCAPE_BYTE){
            printf("Escaped byte found\r\n");
            found_escape = true;
            if ( frame->length - frame_cntr < 5){
                printf("Entered potential crc range\r\n");
                crc_range = true;
            }
            continue;
        }
        if (frame->length - frame_cntr < 4){
            crc_range = true;
            printf("Entered definite crc range\r\n");
        }
        if (crc_range){
            found_escape ? printf("crc16 = 0x%02X\r\n", frame->payload[frame_cntr]^HDLC_ESCAPE_XOR) : printf("crc16 = 0x%02X\r\n", frame->payload[frame_cntr]);
            printf("Crc Marker: %ld\r\n", crc_marker);
            crc16 |= (found_escape ? frame->payload[frame_cntr]^HDLC_ESCAPE_XOR : frame->payload[frame_cntr]) << crc_marker;
            printf("crc16 = 0x%04X\r\n", crc16);
            if (!crc_marker) {
                crc_marker = 8;
            }
            found_escape = false;
        }
        else{
            payload[outbyte_ctr++] = found_escape ? frame->payload[frame_cntr]^HDLC_ESCAPE_XOR : frame->payload[frame_cntr];
            printf("Wrote payload[%ld] = 0x%02X\r\n", outbyte_ctr - 1, payload[outbyte_ctr - 1]);
            found_escape = false;
            if (outbyte_ctr > out_capacity) {
                printf("Payload too long\r\n");
                goto abort;
            }
        }
    }
    *payload_length = outbyte_ctr;
    // // After check if that assumption is true
    // size_t crc_start_from_back = frame->length - 1;
    // size_t crc_length = 2;
    // uint16_t crc16 = (frame->payload[crc_start_from_back - 1] << 8) | frame->payload[crc_start_from_back];
    // printf("Last three bytes: 0x%02X 0x%02X 0x%02X\r\n", frame->payload[crc_start_from_back -1], frame->payload[crc_start_from_back - 2], frame->payload[crc_start_from_back]);
    // if (frame->payload[crc_start_from_back - 1] == HDLC_ESCAPE_BYTE){
    //     printf("First crc was escaped\r\n");
    //     crc16 &= 0xFF00;
    //     crc16 |= frame->payload[crc_start_from_back]^HDLC_ESCAPE_XOR;
    //     ++crc_length;
    //     if (frame->payload[crc_start_from_back - 4] == HDLC_ESCAPE_BYTE){
    //         printf("Second crc was escaped\r\n");
    //         crc16 &= 0x00FF;
    //         crc16 |= (frame->payload[crc_start_from_back - 3]^HDLC_ESCAPE_XOR << 8);
    //         ++crc_length;
    //     }
    // }
    // else if (frame->payload[crc_start_from_back - 3] == HDLC_ESCAPE_BYTE){
    //     crc16 &= 0x00FF;
    //     crc16 |= (frame->payload[crc_start_from_back - 3]^HDLC_ESCAPE_XOR << 8);
    //     printf("Second crc was escaped\r\n");
    //     ++crc_length;
    // }
    // printf("Crc: 0x%04X ", crc16);
    // printf("Crc length: %ld ", (long)crc_length);
    // out_length -= crc_length;
    // printf("out length without flags and crc: %ld\r\n", (long)out_length);

    // for (size_t frame_cntr = 1, outbyte_ctr = 0; frame_cntr < frame->length - 1 - crc_length; frame_cntr++){
    //     if (frame->payload[frame_cntr] == HDLC_ESCAPE_BYTE){
    //         printf("Payload contains escape byte\r\n");
    //         --out_length;
    //     }
    //     else if (frame->payload[frame_cntr-1] == HDLC_ESCAPE_BYTE){
    //         printf("Previous byte was escaped\r\n");
    //         printf("Xoring 0x%02X with 0x%02X, Result: payload[%ld] = 0x%02X\r\n", frame->payload[frame_cntr], HDLC_ESCAPE_XOR, (long)outbyte_ctr, frame->payload[frame_cntr]^HDLC_ESCAPE_XOR);
    //         payload[outbyte_ctr++] = frame->payload[frame_cntr]^HDLC_ESCAPE_XOR;
    //     }
    //     else{
    //         printf("No escape registered in the last byte or in this byte\r\n");
    //         printf("Result: payload[%ld] = 0x%02X\r\n", (long)outbyte_ctr ,frame->payload[frame_cntr]);
    //         payload[outbyte_ctr++] = frame->payload[frame_cntr];
    //     }
    // }
    // uint16_t crc16_calculated = hdlc_crc16(payload, out_length);
    // printf("Decoded %ld bytes\r\n", (long)out_length);
    // if (out_length > out_capacity){
    //     printf("Payload too long\r\n");
    //     goto abort;
    // }
    // *payload_length = out_length;
    // memcpy(payload, frame->payload + 1, out_length);

    return true;

abort:
    if (payload_length){*payload_length = 0;}
    return false;
}
