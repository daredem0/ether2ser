/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/protocol/hdlc_sync.c
 * Purpose: HDLC sync accumulator implementation.
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "hdlc_sync.h"

// Standard library headers
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// Project Headers
#include "hdlc_common.h"
#include "system/error.h"

// Generated headers

void hdlc_sync_acc_init(HDLC_SYNC_ACCUMULATOR_T* accumulator, uint8_t sync_byte)
{
    accumulator->position         = 0;
    accumulator->bit_offset       = 0;
    accumulator->state            = HDLC_SYNC_STATE_HUNTING;
    accumulator->sync_byte        = sync_byte;
    accumulator->sync_accumulator = 0;
}

bool hdlc_sync_acc_process_byte(HDLC_SYNC_ACCUMULATOR_T* accumulator, uint8_t byte)
{
    if (!accumulator)
    {
        return false;
    }
    if (accumulator->position >= RX_HDLC_SYNC_MAX_BUFFER_SIZE)
    {
        return false;
    }
    accumulator->buffer[accumulator->position] = byte;
    accumulator->position++;
    return true;
}

e2s_error_t hdlc_sync_acc_poll(HDLC_SYNC_ACCUMULATOR_T* accumulator, HDLC_FRAME_T* out_frame)
{
    if (accumulator->position < 2 * sizeof(accumulator->sync_accumulator))
    {
        return E2S_OK;
    }
    size_t  out_frame_position = 0;
    uint8_t aligned;
    for (size_t i = 0; i < accumulator->position; i++)
    {
        switch (accumulator->state)
        {
        case HDLC_SYNC_STATE_HUNTING:
            accumulator->sync_accumulator =
                (accumulator->sync_accumulator << 8) | accumulator->buffer[i];
            for (size_t bit_pos = 0; bit_pos < 8; bit_pos++)
            {
                if ((((accumulator->sync_accumulator << bit_pos) >> 8) & 0xFF) ==
                    accumulator->sync_byte)
                {

                    accumulator->state      = HDLC_SYNC_STATE_SYNCING;
                    accumulator->bit_offset = bit_pos;
                    for (size_t j = 0; j < sizeof(accumulator->sync_accumulator); j++)
                    {
                        out_frame->payload[out_frame_position] =
                            (accumulator->buffer[i - 1 + j] << accumulator->bit_offset) |
                            (accumulator->buffer[i + j] >> (8 - accumulator->bit_offset));
                        ++(out_frame->length);
                        ++out_frame_position;
                    }
                    break;
                }
            }
            break;
        case HDLC_SYNC_STATE_SYNCING:
            if (accumulator->bit_offset == 0)
            {
                aligned = accumulator->buffer[i];
            }
            else
            {
                aligned = (accumulator->buffer[i] << accumulator->bit_offset) |
                          (accumulator->buffer[i + 1] >> (8 - accumulator->bit_offset));
            }
            out_frame->payload[out_frame_position] = aligned;
            ++(out_frame->length);
            ++out_frame_position;
            accumulator->state = HDLC_SYNC_STATE_SYNCED;
            break;
        case HDLC_SYNC_STATE_SYNCED:
            aligned;
            if (accumulator->bit_offset == 0)
            {
                aligned = accumulator->buffer[i];
            }
            else
            {
                aligned = (accumulator->buffer[i] << accumulator->bit_offset) |
                          (accumulator->buffer[i + 1] >> (8 - accumulator->bit_offset));
            }
            out_frame->payload[out_frame_position] = aligned;
            ++(out_frame->length);
            ++out_frame_position;
            if (aligned == accumulator->sync_byte)
            {
                printf("FRAME READY\n");
                return E2S_ERR_HDLC_ACC_FRAME_READY;
            }
            break;
        default:
            return E2S_OK;
        }
    }

    return E2S_OK;
}
