/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/protocol/hdlc_sync.c
 * Purpose: HDLC sync accumulator implementation.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "hdlc_sync.h"

// Standard library headers
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Project Headers
#include "hdlc_common.h"
#include "system/error.h"

// Generated headers

void hdlc_sync_acc_init(HDLC_SYNC_ACCUMULATOR_T* accumulator, uint8_t sync_byte)
{
    accumulator->position               = 0;
    accumulator->processed              = 0;
    accumulator->candidate_start        = 0;
    accumulator->candidate_end          = 0;
    accumulator->candidate_valid        = false;
    accumulator->candidate_i            = 0;
    accumulator->candidate_bit_pos      = 0;
    accumulator->resume_pending         = false;
    accumulator->resume_i               = 0;
    accumulator->resume_bit_pos         = 0;
    accumulator->bit_offset             = 0;
    accumulator->state                  = HDLC_SYNC_STATE_HUNTING;
    accumulator->sync_byte              = sync_byte;
    accumulator->sync_accumulator       = 0;
    accumulator->lookahead_wait_syncing = 0;
    accumulator->lookahead_wait_synced  = 0;
    accumulator->frame_ready_count      = 0;
    accumulator->consume_count          = 0;
    accumulator->hardcap_drop_events    = 0;
    accumulator->hardcap_drop_bytes     = 0;
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
    if (!accumulator || !out_frame || !out_frame->payload || out_frame->capacity == 0)
    {
        return E2S_OK;
    }
    if (accumulator->position == 0)
    {
        return E2S_OK;
    }

    // only needed when aligned byte needs lookahead
    if (accumulator->state != HDLC_SYNC_STATE_HUNTING && accumulator->bit_offset != 0 &&
        accumulator->position < 2)
    {
        return E2S_OK;
    }

    e2s_error_t result     = E2S_OK;
    size_t      scan_index = accumulator->processed;
    uint8_t     aligned    = 0;
    for (; scan_index < accumulator->position; scan_index++)
    {
        switch (accumulator->state)
        {
        case HDLC_SYNC_STATE_HUNTING:
            accumulator->sync_accumulator =
                (accumulator->sync_accumulator << 8) | accumulator->buffer[scan_index];
            if (scan_index >= 1 && (scan_index + 1) < accumulator->position)
            {
                for (size_t bit_pos = 0; bit_pos < 8; bit_pos++)
                {
                    if ((((accumulator->sync_accumulator << bit_pos) >> 8) & 0xFF) ==
                        accumulator->sync_byte)
                    {
                        accumulator->state             = HDLC_SYNC_STATE_SYNCING;
                        accumulator->bit_offset        = bit_pos;
                        accumulator->candidate_start   = scan_index - 1;
                        accumulator->candidate_i       = scan_index;
                        accumulator->candidate_bit_pos = (uint8_t)bit_pos;
                        out_frame->length              = 0;
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
                                (accumulator->buffer[scan_index - 1 + j]
                                 << accumulator->bit_offset) |
                                (accumulator->buffer[scan_index + j] >>
                                 (8 - accumulator->bit_offset));
                        }
                        break;
                    }
                }
            }
            else if ((scan_index + 1) >= accumulator->position)
            {
                scan_index++;
                goto out;
            }
            break;
        case HDLC_SYNC_STATE_SYNCING:
            if (accumulator->bit_offset != 0 && (scan_index + 1) >= accumulator->position)
            {
                accumulator->lookahead_wait_syncing++;
                goto out;
            }
            if (accumulator->bit_offset == 0)
            {
                aligned = accumulator->buffer[scan_index];
            }
            else
            {
                aligned = (accumulator->buffer[scan_index] << accumulator->bit_offset) |
                          (accumulator->buffer[scan_index + 1] >> (8 - accumulator->bit_offset));
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
            if (accumulator->bit_offset != 0 && (scan_index + 1) >= accumulator->position)
            {
                accumulator->lookahead_wait_synced++;
                goto out;
            }
            if (accumulator->bit_offset == 0)
            {
                aligned = accumulator->buffer[scan_index];
            }
            else
            {
                aligned = (accumulator->buffer[scan_index] << accumulator->bit_offset) |
                          (accumulator->buffer[scan_index + 1] >> (8 - accumulator->bit_offset));
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
                accumulator->frame_ready_count++;
                accumulator->state            = HDLC_SYNC_STATE_HUNTING;
                accumulator->sync_accumulator = 0;
                accumulator->candidate_end    = scan_index + 1;
                accumulator->candidate_valid  = true;
                result                        = E2S_ERR_HDLC_ACC_FRAME_READY;
                scan_index++;
                goto out;
            }
            break;
        default:
            return E2S_OK;
        }
    }

out:
    accumulator->processed = scan_index;
    if (result == E2S_ERR_HDLC_ACC_FRAME_READY)
    {
        accumulator->processed = 0;
        return result;
    }
    size_t keep = 0;
    if (accumulator->state == HDLC_SYNC_STATE_HUNTING)
    {
        // Hunting checks a cross-byte sync pattern, so keep 2 bytes overlap.
        keep = 2;
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

    (void)accept;
    {
        // With bit-stuffed HDLC, a detected closing flag is definitive. Always advance
        // past the candidate frame boundary and continue scanning from there.
        size_t drop = accumulator->candidate_end;
        if (drop > accumulator->position)
        {
            drop = accumulator->position;
        }
        if (drop > 0)
        {
            size_t remaining = accumulator->position - drop;
            accumulator->consume_count++;
            memmove(accumulator->buffer, accumulator->buffer + drop, remaining);
            accumulator->position = remaining;
        }
    }

    accumulator->processed         = 0;
    accumulator->candidate_start   = 0;
    accumulator->candidate_end     = 0;
    accumulator->candidate_valid   = false;
    accumulator->candidate_i       = 0;
    accumulator->candidate_bit_pos = 0;
    accumulator->bit_offset        = 0;
    accumulator->state             = HDLC_SYNC_STATE_HUNTING;
    accumulator->sync_accumulator  = 0;
    accumulator->resume_pending    = false;
    accumulator->resume_i          = 0;
    accumulator->resume_bit_pos    = 0;

    // Hard cap: if buffer is near full, drop oldest bytes to keep bounded.
    if (accumulator->position >= (RX_HDLC_SYNC_MAX_BUFFER_SIZE - 16))
    {
        size_t keep = 16;
        size_t drop = accumulator->position - keep;
        accumulator->hardcap_drop_events++;
        accumulator->hardcap_drop_bytes += (uint32_t)drop;
        memmove(accumulator->buffer, accumulator->buffer + drop, keep);
        accumulator->position       = keep;
        accumulator->processed      = 0;
        accumulator->resume_pending = false;
    }
}
