/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/protocol/hdlc_decoder.c
 * Purpose: HDLC decoder (deframing, unescaping, and CRC check).
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "hdlc_decoder.h"

// Standard library headers
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Project Headers
#include "hdlc_common.h"
#include "system/common.h"

// Generated headers

#define HDLC_BIT_STUFF_ONES_LIMIT 5U

typedef struct
{
    uint8_t ones_run;
    size_t  raw_bit_index;
    bool    skip_next_zero;
    bool    lsb_first;
} hdlc_decoder_t;
typedef enum
{
    HDLC_BIT_OK,
    HDLC_BIT_EOF,
    HDLC_BIT_ERR
} hdlc_decoder_bit_type_t;

bool hdlc_decode_byte(const HDLC_FRAME_T* frame, uint8_t* payload, const size_t out_capacity,
                      size_t* payload_length)
{
    if (frame == NULL || payload == NULL || payload_length == NULL || out_capacity == 0 ||
        frame->payload[0] != HDLC_FLAG_BYTE || frame->payload[frame->length - 1] != HDLC_FLAG_BYTE)
    {
        LOG_DEBUG("Invalid frame\r\n");
        goto abort;
    }

    bool   found_escape = false;
    size_t outbyte_ctr  = 0;
    for (size_t frame_cntr = 1; frame_cntr < frame->length - 1; frame_cntr++)
    {
        if (frame->payload[frame_cntr] == HDLC_ESCAPE_BYTE)
        {
            found_escape = true;
            continue;
        }
        if (outbyte_ctr >= out_capacity)
        {
            LOG_DEBUG("Payload too long\r\n");
            goto abort;
        }
        payload[outbyte_ctr++] = found_escape ? frame->payload[frame_cntr] ^ HDLC_ESCAPE_XOR
                                              : frame->payload[frame_cntr];
        found_escape           = false;
    }

    if (outbyte_ctr < 2)
    {
        goto abort;
    }

    uint16_t crc16  = payload[outbyte_ctr - 2] << 8 | payload[outbyte_ctr - 1];
    *payload_length = (outbyte_ctr - 2);

    // Check crc
    uint16_t recovered_crc = hdlc_crc16(payload, *payload_length);
    if (crc16 != recovered_crc)
    {
        goto abort;
    }

    return true;

abort:
    if (payload_length)
    {
        *payload_length = 0;
    }
    return false;
}

#define HDLC_DEC_BYTE_IDX(raw_bit_index) (1 + ((raw_bit_index) / CHAR_BIT))
#define HDLC_DEC_BIT_IDX(raw_bit_index) ((raw_bit_index) % CHAR_BIT)
#define HDLC_DEC_BIT_POS(decoder)                                        \
    ((decoder)->lsb_first ? (HDLC_DEC_BIT_IDX((decoder)->raw_bit_index)) \
                          : ((7 - HDLC_DEC_BIT_IDX((decoder)->raw_bit_index))))
#define HDLC_DEC_GET_OUT_BIT(decoder) \
    (frame->payload[HDLC_DEC_BYTE_IDX((decoder)->raw_bit_index)] >> HDLC_DEC_BIT_POS((decoder)) & 1)

static hdlc_decoder_bit_type_t hdlc_get_bit(hdlc_decoder_t* decoder, const HDLC_FRAME_T* frame,
                                            uint8_t* out_bit)
{
    if (!decoder || !frame || !out_bit)
    {
        return HDLC_BIT_ERR;
    }
    size_t raw_bits_total = (frame->length > 2) ? (frame->length - 2) * CHAR_BIT : 0;

    // TODO: This has to be tested on target
    while (decoder->raw_bit_index < raw_bits_total)
    {
        uint8_t raw_bit = HDLC_DEC_GET_OUT_BIT(decoder);
        ++decoder->raw_bit_index;
        if (decoder->skip_next_zero)
        {
            if (raw_bit != 0)
            {
                return HDLC_BIT_ERR;
            }
            decoder->skip_next_zero = false;
            decoder->ones_run       = 0;
            continue;
        }
        *out_bit = raw_bit;
        if (raw_bit)
        {
            if ((++decoder->ones_run) == HDLC_BIT_STUFF_ONES_LIMIT)
            {
                decoder->skip_next_zero = true;
            }
        }
        else
        {
            decoder->ones_run = 0;
        }
        return HDLC_BIT_OK;
    }
    return HDLC_BIT_EOF;
}

static hdlc_decoder_bit_type_t hdlc_get_byte(hdlc_decoder_t* decoder, const HDLC_FRAME_T* frame,
                                             uint8_t* byte)
{
    *byte = 0;
    for (uint8_t i = 0; i < CHAR_BIT; i++)
    {
        size_t                  bit_position = 0;
        uint8_t                 bit;
        hdlc_decoder_bit_type_t bit_result = hdlc_get_bit(decoder, frame, &bit);
        if (bit_result != HDLC_BIT_OK)
        {
            return bit_result;
        }
        bit_position = decoder->lsb_first ? i : (CHAR_BIT - 1U - i);
        *byte |= bit << bit_position;
    }
    return HDLC_BIT_OK;
}

bool hdlc_decode(const HDLC_FRAME_T* frame, uint8_t* payload, const size_t out_capacity,
                 size_t* payload_length, bool lsb_first)
{
    if (frame == NULL || payload == NULL || payload_length == NULL || out_capacity == 0 ||
        frame->length < 2 || frame->payload[0] != HDLC_FLAG_BYTE ||
        frame->payload[frame->length - 1] != HDLC_FLAG_BYTE)
    {
        LOG_DEBUG("Invalid frame\r\n");
        goto abort;
    }

    size_t         outbyte_ctr = 0;
    hdlc_decoder_t decoder     = {
            .raw_bit_index = 0, .ones_run = 0, .skip_next_zero = false, .lsb_first = lsb_first};
    for (;;)
    {
        uint8_t                 out_byte;
        hdlc_decoder_bit_type_t bit_result = hdlc_get_byte(&decoder, frame, &out_byte);
        if (bit_result == HDLC_BIT_EOF)
        {
            break;
        }
        if (bit_result == HDLC_BIT_ERR)
        {
            LOG_DEBUG("Invalid frame\r\n");
            goto abort;
        }
        if (outbyte_ctr >= out_capacity)
        {
            LOG_DEBUG("Payload too long\r\n");
            goto abort;
        }
        payload[outbyte_ctr++] = out_byte;
    }

    if (outbyte_ctr < 2)
    {
        goto abort;
    }

    uint16_t crc16  = payload[outbyte_ctr - 2] << CHAR_BIT | payload[outbyte_ctr - 1];
    *payload_length = (outbyte_ctr - 2);

    // Check crc
    uint16_t recovered_crc = hdlc_crc16(payload, *payload_length);
    if (crc16 != recovered_crc)
    {
        LOG_DEBUG("CRC mismatch\r\n");
        goto abort;
    }

    return true;

abort:
    if (payload_length)
    {
        *payload_length = 0;
    }
    return false;
}
