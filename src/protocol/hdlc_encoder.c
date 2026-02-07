/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/protocol/hdlc_encoder.c
 * Purpose: HDLC encoder (framing, escaping, and CRC append).
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "hdlc_encoder.h"

// Standard library headers
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Project Headers
#include "hdlc_common.h"

// Generated headers

bool lsb_first = true;

#define HDLC_TRY_PUT_BYTE(byte, frame, error_handling) \
    do                                                 \
    {                                                  \
        if ((frame)->length + 1 > (frame)->capacity)   \
        {                                              \
            goto error_handling;                       \
        }                                              \
        (frame)->payload[(frame)->length++] = byte;    \
    } while (0)

static bool hdlc_escape_if_needed(uint8_t byte, HDLC_FRAME_T* frame)
{
    if (byte == HDLC_FLAG_BYTE || byte == HDLC_ESCAPE_BYTE)
    {
        HDLC_TRY_PUT_BYTE(HDLC_ESCAPE_BYTE, frame, abort);
        HDLC_TRY_PUT_BYTE(byte ^ HDLC_ESCAPE_XOR, frame, abort);
    }
    else
    {
        HDLC_TRY_PUT_BYTE(byte, frame, abort);
    }
    return true;
abort:
    return false;
}

bool hdlc_encode_byte(const uint8_t* payload, const size_t payload_length, HDLC_FRAME_T* frame)
{
    if (frame == NULL || (payload == NULL && payload_length > 0) || (frame->capacity < 2) ||
        frame->length != 0)
    {
        goto abort;
    }

    // Write opening flag
    HDLC_TRY_PUT_BYTE(HDLC_FLAG_BYTE, frame, abort);

    // Write data (only if there is actual data to write)
    for (size_t i = 0; i < payload_length; i++)
    {
        if (!hdlc_escape_if_needed(payload[i], frame))
        {
            goto abort;
        }
    }

    uint16_t crc16 = hdlc_crc16(payload, payload_length);
    if (!hdlc_escape_if_needed((crc16 >> CHAR_BIT) & 0xFF, frame))
    {
        goto abort;
    }
    if (!hdlc_escape_if_needed(crc16 & 0xFF, frame))
    {
        goto abort;
    }

    // Write closing flag
    HDLC_TRY_PUT_BYTE(HDLC_FLAG_BYTE, frame, abort);
    return true;
abort:
    if (frame)
    {
        frame->length = 0;
    }
    return false;
}

typedef struct
{
    uint8_t out_byte;
    uint8_t out_bits_used;
    uint8_t ones_run;
    bool    lsb_first;
} hdlc_encoder_t;

#define SHIFT_IN_BIT(out_byte, bitorder, bit, bitpos) \
    (bitorder) ? ((out_byte) | ((bit) << (bitpos))) : ((out_byte) | ((bit) << (7 - (bitpos))))

#define ENCODER_TRY_FLUSH_BYTE_OUT(encoder, frame, error_handling)             \
    do                                                                         \
    {                                                                          \
        if ((++(encoder)->out_bits_used) == CHAR_BIT)                          \
        {                                                                      \
            HDLC_TRY_PUT_BYTE(((encoder)->out_byte), (frame), error_handling); \
            (encoder)->out_bits_used = 0;                                      \
            (encoder)->out_byte      = 0;                                      \
        }                                                                      \
    } while (0)

#define ENCODER_ZERO_PAD_AND_DRAIN(encoder, frame, error_handling)         \
    do                                                                     \
    {                                                                      \
        if ((encoder)->out_bits_used > 0)                                  \
        {                                                                  \
            HDLC_TRY_PUT_BYTE((encoder)->out_byte, frame, error_handling); \
            (encoder)->out_byte      = 0;                                  \
            (encoder)->ones_run      = 0;                                  \
            (encoder)->out_bits_used = 0;                                  \
        }                                                                  \
    } while (0)

static bool hdlc_put_bit(hdlc_encoder_t* encoder, bool bit, HDLC_FRAME_T* frame)
{
    uint8_t bitmask = bit ? (encoder->ones_run++, 1) : (encoder->ones_run = 0, 0);
    encoder->out_byte =
        SHIFT_IN_BIT(encoder->out_byte, encoder->lsb_first, bitmask, encoder->out_bits_used);

    ENCODER_TRY_FLUSH_BYTE_OUT(encoder, frame, abort);
    if (encoder->ones_run == 5)
    {
        encoder->out_byte =
            SHIFT_IN_BIT(encoder->out_byte, encoder->lsb_first, 0, encoder->out_bits_used);
        encoder->ones_run = 0;
        ENCODER_TRY_FLUSH_BYTE_OUT(encoder, frame, abort);
    }
    return true;
abort:
    return false;
}

static bool hdlc_put_byte(hdlc_encoder_t* encoder, uint8_t byte, HDLC_FRAME_T* frame)
{
    for (uint8_t i = 0; i < CHAR_BIT; i++)
    {
        uint8_t bit_pos = encoder->lsb_first ? i : (7 - i);
        if (!hdlc_put_bit(encoder, byte & (1 << bit_pos), frame))
        {
            return false;
        }
    }
    return true;
}

bool hdlc_encode(const uint8_t* payload, const size_t payload_length, HDLC_FRAME_T* frame)
{
    if (frame == NULL || (payload == NULL && payload_length > 0) || (frame->capacity < 2) ||
        frame->length != 0)
    {
        goto abort;
    }

    // Write opening flag
    HDLC_TRY_PUT_BYTE(HDLC_FLAG_BYTE, frame, abort);

    hdlc_encoder_t encoder = {
        .out_byte = 0, .out_bits_used = 0, .ones_run = 0, .lsb_first = lsb_first};
    // Write data (only if there is actual data to write)
    for (size_t i = 0; i < payload_length; i++)
    {
        if (!hdlc_put_byte(&encoder, payload[i], frame))
        {
            goto abort;
        }
    }

    uint16_t crc16 = hdlc_crc16(payload, payload_length);
    if (!hdlc_put_byte(&encoder, (crc16 >> CHAR_BIT) & 0xFF, frame) ||
        !hdlc_put_byte(&encoder, crc16 & 0xFF, frame))
    {
        goto abort;
    }

    // Flush out remainig bits (if any)
    ENCODER_ZERO_PAD_AND_DRAIN(&encoder, frame, abort);

    // Write closing flag
    HDLC_TRY_PUT_BYTE(HDLC_FLAG_BYTE, frame, abort);
    return true;
abort:
    if (frame)
    {
        frame->length = 0;
    }
    return false;
}
