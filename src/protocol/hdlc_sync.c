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
#include <string.h>

// Project Headers
#include "hdlc_common.h"
#include "system/error.h"

// Generated headers

void hdlc_sync_acc_init(HDLC_SYNC_ACCUMULATOR_T* accumulator, uint8_t sync_byte)
{
    accumulator->position         = 0;
    accumulator->processed        = 0;
    accumulator->candidate_start  = 0;
    accumulator->candidate_end    = 0;
    accumulator->candidate_valid  = false;
    accumulator->candidate_i      = 0;
    accumulator->candidate_bit_pos = 0;
    accumulator->resume_pending   = false;
    accumulator->resume_i         = 0;
    accumulator->resume_bit_pos   = 0;
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
    static size_t frame_ready_count = 0;
    if (!accumulator || !out_frame || !out_frame->payload || out_frame->capacity == 0)
    {
        return E2S_OK;
    }
    if (accumulator->position < 2 * sizeof(accumulator->sync_accumulator))
    {
        return E2S_OK;
    }
    e2s_error_t result  = E2S_OK;
    size_t      i       = accumulator->processed;
    uint8_t     aligned = 0;
    for (; i < accumulator->position; i++)
    {
        switch (accumulator->state)
        {
        case HDLC_SYNC_STATE_HUNTING:
            accumulator->sync_accumulator =
                (accumulator->sync_accumulator << 8) | accumulator->buffer[i];
            if (i >= 1 && (i + 1) < accumulator->position)
            {
                size_t bit_start = 0;
                if (accumulator->resume_pending && i == accumulator->resume_i)
                {
                    bit_start = accumulator->resume_bit_pos;
                    accumulator->resume_pending = false;
                }
                for (size_t bit_pos = bit_start; bit_pos < 8; bit_pos++)
                {
                    if ((((accumulator->sync_accumulator << bit_pos) >> 8) & 0xFF) ==
                        accumulator->sync_byte)
                    {
                        accumulator->state      = HDLC_SYNC_STATE_SYNCING;
                        accumulator->bit_offset = bit_pos;
                        accumulator->candidate_start = (i >= 1) ? (i - 1) : 0;
                        accumulator->candidate_i     = i;
                        accumulator->candidate_bit_pos = (uint8_t)bit_pos;
                        out_frame->length       = 0;
                        for (size_t j = 0; j < sizeof(accumulator->sync_accumulator); j++)
                        {
                            if (out_frame->length >= out_frame->capacity)
                            {
                                out_frame->length       = 0;
                                accumulator->state      = HDLC_SYNC_STATE_HUNTING;
                                accumulator->bit_offset = 0;
                                return E2S_ERR_HDLC_DECODE_PAYLOAD_TOO_LONG;
                            }
                            out_frame->payload[out_frame->length++] =
                                (accumulator->buffer[i - 1 + j] << accumulator->bit_offset) |
                                (accumulator->buffer[i + j] >> (8 - accumulator->bit_offset));
                        }
                        break;
                    }
                }
            }
            else if ((i + 1) >= accumulator->position)
            {
                i++;
                goto out;
            }
            break;
        case HDLC_SYNC_STATE_SYNCING:
            if (accumulator->bit_offset != 0 && (i + 1) >= accumulator->position)
            {
                goto out;
            }
            if (accumulator->bit_offset == 0)
            {
                aligned = accumulator->buffer[i];
            }
            else
            {
                aligned = (accumulator->buffer[i] << accumulator->bit_offset) |
                          (accumulator->buffer[i + 1] >> (8 - accumulator->bit_offset));
            }
            if (out_frame->length >= out_frame->capacity)
            {
                out_frame->length       = 0;
                accumulator->state      = HDLC_SYNC_STATE_HUNTING;
                accumulator->bit_offset = 0;
                return E2S_ERR_HDLC_DECODE_PAYLOAD_TOO_LONG;
            }
            out_frame->payload[out_frame->length++] = aligned;
            accumulator->state                      = HDLC_SYNC_STATE_SYNCED;
            break;
        case HDLC_SYNC_STATE_SYNCED:
            if (accumulator->bit_offset != 0 && (i + 1) >= accumulator->position)
            {
                goto out;
            }
            if (accumulator->bit_offset == 0)
            {
                aligned = accumulator->buffer[i];
            }
            else
            {
                aligned = (accumulator->buffer[i] << accumulator->bit_offset) |
                          (accumulator->buffer[i + 1] >> (8 - accumulator->bit_offset));
            }
            if (out_frame->length >= out_frame->capacity)
            {
                out_frame->length       = 0;
                accumulator->state      = HDLC_SYNC_STATE_HUNTING;
                accumulator->bit_offset = 0;
                return E2S_ERR_HDLC_DECODE_PAYLOAD_TOO_LONG;
            }
            out_frame->payload[out_frame->length++] = aligned;
            if (aligned == accumulator->sync_byte)
            {
                ++frame_ready_count;
                printf("%zu FRAME READY\n", frame_ready_count);
                accumulator->state            = HDLC_SYNC_STATE_HUNTING;
                accumulator->sync_accumulator = 0;
                accumulator->candidate_end    = i + 1;
                accumulator->candidate_valid  = true;
                result                        = E2S_ERR_HDLC_ACC_FRAME_READY;
                i++;
                goto out;
            }
            break;
        default:
            return E2S_OK;
        }
    }

out:
    accumulator->processed = i;
    if (result == E2S_ERR_HDLC_ACC_FRAME_READY)
    {
        accumulator->processed = 0;
        return result;
    }
    size_t keep            = 0;
    if (accumulator->state == HDLC_SYNC_STATE_HUNTING)
    {
        keep = 1;
    }
    else if (accumulator->bit_offset != 0)
    {
        keep = 1;
    }

    if (accumulator->processed > keep)
    {
        size_t drop      = accumulator->processed - keep;
        size_t remaining = accumulator->position - drop;
        memmove(accumulator->buffer, accumulator->buffer + drop, remaining);
        accumulator->position = remaining;
    }
    // Any retained overlap bytes must be reprocessed on next poll.
    accumulator->processed = 0;
    return result;
}

void hdlc_sync_acc_consume_candidate(HDLC_SYNC_ACCUMULATOR_T* accumulator, bool accept)
{
    if (!accumulator || !accumulator->candidate_valid)
    {
        return;
    }

    if (accept)
    {
        size_t drop = accumulator->candidate_end;
        if (drop > accumulator->position)
        {
            drop = accumulator->position;
        }
        if (drop > 0)
        {
            size_t remaining = accumulator->position - drop;
            memmove(accumulator->buffer, accumulator->buffer + drop, remaining);
            accumulator->position = remaining;
        }
    }
    else
    {
        // Retry next bit-offset at the same byte if possible.
        uint8_t next_bit = (uint8_t)(accumulator->candidate_bit_pos + 1);
        if (next_bit < 8)
        {
            accumulator->resume_pending = true;
            accumulator->resume_i       = accumulator->candidate_i;
            accumulator->resume_bit_pos = next_bit;
        }
        else
        {
            // No more bit offsets for this byte, drop one byte to ensure progress.
            size_t drop = accumulator->candidate_start + 1;
            if (drop > accumulator->position)
            {
                drop = accumulator->position;
            }
            if (drop > 0)
            {
                size_t remaining = accumulator->position - drop;
                memmove(accumulator->buffer, accumulator->buffer + drop, remaining);
                accumulator->position = remaining;
            }
        }
    }

    accumulator->processed        = 0;
    accumulator->candidate_start  = 0;
    accumulator->candidate_end    = 0;
    accumulator->candidate_valid  = false;
    accumulator->candidate_i      = 0;
    accumulator->candidate_bit_pos = 0;
    accumulator->bit_offset       = 0;
    accumulator->state            = HDLC_SYNC_STATE_HUNTING;
    accumulator->sync_accumulator = 0;

    // Hard cap: if buffer is near full, drop oldest bytes to keep bounded.
    if (accumulator->position >= (RX_HDLC_SYNC_MAX_BUFFER_SIZE - 16))
    {
        size_t keep = 16;
        size_t drop = accumulator->position > keep ? (accumulator->position - keep) : 0;
        if (drop > 0)
        {
            memmove(accumulator->buffer, accumulator->buffer + drop, keep);
            accumulator->position = keep;
        }
        accumulator->processed = 0;
        accumulator->resume_pending = false;
    }
}
