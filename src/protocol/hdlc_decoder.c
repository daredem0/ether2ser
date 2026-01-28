/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/protocol/hdlc_decoder.c
 * Purpose: HDLC decoder (deframing, unescaping, and CRC check).
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#include "hdlc_decoder.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "hdlc_common.h"


bool hdlc_decode(const HDLC_FRAME_T *frame, uint8_t *payload, const size_t out_capacity, size_t *payload_length){
    if (frame == NULL || payload == NULL || payload_length == NULL || out_capacity == 0 ||
        frame->payload[0] != HDLC_FLAG_BYTE ||
        frame->payload[frame->length - 1] != HDLC_FLAG_BYTE){
        printf("Invalid frame\r\n");
        goto abort;
    }

    bool found_escape = false;
    size_t outbyte_ctr = 0;
    for (size_t frame_cntr = 1; frame_cntr < frame->length - 1; frame_cntr++){
        if (frame->payload[frame_cntr] == HDLC_ESCAPE_BYTE){
            found_escape = true;
            continue;
        }
        if (outbyte_ctr >= out_capacity) {
            printf("Payload too long\r\n");
            goto abort;
        }
        payload[outbyte_ctr++] = found_escape ? frame->payload[frame_cntr]^HDLC_ESCAPE_XOR : frame->payload[frame_cntr];
        found_escape = false;
    }

    uint16_t crc16 = payload[outbyte_ctr - 2] << 8 | payload[outbyte_ctr-1];
    *payload_length = (outbyte_ctr - 2);

    // Check crc
    uint16_t recovered_crc = hdlc_crc16(payload, *payload_length);
    if (crc16 != recovered_crc){
        goto abort;
    }


    return true;

abort:
    if (payload_length){*payload_length = 0;}
    return false;
}
