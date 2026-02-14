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

#define HDLC_SYNC_MAX_REASONABLE_FRAME_SIZE_BYTES 2048U
#define HDLC_SYNC_SHORT_BUFFER_SEARCH_LIMIT 64U

static void hdlc_sync_drop_prefix(HDLC_SYNC_ACCUMULATOR_T* accumulator, size_t drop_count)
{
    if (!accumulator || drop_count == 0)
    {
        return;
    }

    if (drop_count >= accumulator->position)
    {
        accumulator->position  = 0;
        accumulator->processed = 0;
        return;
    }

    size_t remaining = accumulator->position - drop_count;
    memmove(accumulator->buffer, accumulator->buffer + drop_count, remaining);
    accumulator->position = remaining;
}

static void hdlc_sync_reset_hunting_state(HDLC_SYNC_ACCUMULATOR_T* accumulator)
{
    if (!accumulator)
    {
        return;
    }

    accumulator->processed         = 0;
    accumulator->candidate_start   = 0;
    accumulator->candidate_end     = 0;
    accumulator->candidate_valid   = false;
    accumulator->state             = HDLC_SYNC_STATE_HUNTING;
    accumulator->bit_offset        = 0;
    accumulator->align_shift_right = false;
}

static void hdlc_sync_reject_oversized_candidate(HDLC_SYNC_ACCUMULATOR_T* accumulator,
                                                  HDLC_FRAME_T* out_frame, size_t* scan_index)
{
    if (!accumulator || !out_frame || !scan_index)
    {
        return;
    }

    // Candidate grew far beyond expected frame sizes; treat it as false lock and
    // retry from the next raw byte so alternate alignments can be tested.
    size_t drop = accumulator->candidate_start + 1;
    if (drop > accumulator->position)
    {
        drop = accumulator->position;
    }

    hdlc_sync_drop_prefix(accumulator, drop);
    hdlc_sync_reset_hunting_state(accumulator);
    out_frame->length = 0;
    *scan_index       = 0;
}

static bool hdlc_sync_get_aligned_byte(const HDLC_SYNC_ACCUMULATOR_T* accumulator, size_t raw_index,
                                       uint8_t bit_offset, bool shift_right, uint8_t* out_byte)
{
    if (!accumulator || !out_byte || raw_index >= accumulator->position || bit_offset >= 8)
    {
        return false;
    }

    if (bit_offset == 0)
    {
        *out_byte = accumulator->buffer[raw_index];
        return true;
    }

    if ((raw_index + 1) >= accumulator->position)
    {
        return false;
    }

    if (shift_right)
    {
        *out_byte = (uint8_t)((uint8_t)(accumulator->buffer[raw_index] >> bit_offset) |
                              (uint8_t)(accumulator->buffer[raw_index + 1] << (8 - bit_offset)));
    }
    else
    {
        *out_byte = (uint8_t)((uint8_t)(accumulator->buffer[raw_index] << bit_offset) |
                              (uint8_t)(accumulator->buffer[raw_index + 1] >> (8 - bit_offset)));
    }
    return true;
}

static bool hdlc_sync_find_opening_candidate(const HDLC_SYNC_ACCUMULATOR_T* accumulator,
                                             size_t scan_index, bool allow_left_shift,
                                             size_t* out_start_index, uint8_t* out_bit_pos,
                                             bool* out_shift_right)
{
    if (!accumulator || !out_start_index || !out_bit_pos || !out_shift_right ||
        accumulator->position < 2)
    {
        return false;
    }

    size_t start = (scan_index > 0) ? (scan_index - 1) : 0;
    if (start >= (accumulator->position - 1))
    {
        return false;
    }

    uint8_t aligned = 0;
    for (; start < (accumulator->position - 1); ++start)
    {
        // Byte-aligned first.
        if (hdlc_sync_get_aligned_byte(accumulator, start, 0, false, &aligned) &&
            aligned == accumulator->sync_byte)
        {
            *out_start_index = start;
            *out_bit_pos     = 0;
            *out_shift_right = false;
            return true;
        }

        // Prefer right-shift alignment for non-zero offsets.
        for (uint8_t bit_pos = 1; bit_pos < 8; ++bit_pos)
        {
            if (hdlc_sync_get_aligned_byte(accumulator, start, bit_pos, true, &aligned) &&
                aligned == accumulator->sync_byte)
            {
                *out_start_index = start;
                *out_bit_pos     = bit_pos;
                *out_shift_right = true;
                return true;
            }
        }

        if (allow_left_shift)
        {
            for (uint8_t bit_pos = 1; bit_pos < 8; ++bit_pos)
            {
                if (hdlc_sync_get_aligned_byte(accumulator, start, bit_pos, false, &aligned) &&
                    aligned == accumulator->sync_byte)
                {
                    *out_start_index = start;
                    *out_bit_pos     = bit_pos;
                    *out_shift_right = false;
                    return true;
                }
            }
        }
    }

    return false;
}

static bool hdlc_sync_find_complete_candidate_short(const HDLC_SYNC_ACCUMULATOR_T* accumulator,
                                                    size_t scan_index, size_t* out_start_index,
                                                    uint8_t* out_bit_pos, bool* out_shift_right)
{
    if (!accumulator || !out_start_index || !out_bit_pos || !out_shift_right ||
        accumulator->position < 4 || accumulator->position > HDLC_SYNC_SHORT_BUFFER_SEARCH_LIMIT)
    {
        return false;
    }

    size_t start = (scan_index > 0) ? (scan_index - 1) : 0;
    if (start >= (accumulator->position - 1))
    {
        return false;
    }

    bool    found_best      = false;
    size_t  best_start      = 0;
    uint8_t best_bit_pos    = 0;
    bool    best_shift      = false;
    size_t  best_frame_size = 0;
    uint8_t start_byte      = 0;
    uint8_t probe_byte      = 0;

    for (; start < (accumulator->position - 1); ++start)
    {
        for (uint8_t bit_pos = 0; bit_pos < 8; ++bit_pos)
        {
            for (uint8_t mode = 0; mode < 2; ++mode)
            {
                bool shift_right = (mode != 0);
                if (bit_pos == 0 && shift_right)
                {
                    continue;
                }

                if (!hdlc_sync_get_aligned_byte(accumulator, start, bit_pos, shift_right,
                                                &start_byte) ||
                    start_byte != accumulator->sync_byte)
                {
                    continue;
                }

                size_t frame_size = 1;
                for (size_t probe = start + 1; probe < accumulator->position; ++probe)
                {
                    if (!hdlc_sync_get_aligned_byte(accumulator, probe, bit_pos, shift_right,
                                                    &probe_byte))
                    {
                        break;
                    }
                    frame_size++;
                    if (probe_byte == accumulator->sync_byte && frame_size >= 4)
                    {
                        if (!found_best || frame_size > best_frame_size)
                        {
                            found_best      = true;
                            best_start      = start;
                            best_bit_pos    = bit_pos;
                            best_shift      = shift_right;
                            best_frame_size = frame_size;
                        }
                        break;
                    }
                }
            }
        }
    }

    if (!found_best)
    {
        return false;
    }

    *out_start_index = best_start;
    *out_bit_pos     = best_bit_pos;
    *out_shift_right = best_shift;
    return true;
}

void hdlc_sync_acc_init(HDLC_SYNC_ACCUMULATOR_T* accumulator, uint8_t sync_byte)
{
    accumulator->position               = 0;
    accumulator->processed              = 0;
    accumulator->candidate_start        = 0;
    accumulator->candidate_end          = 0;
    accumulator->candidate_valid        = false;
    accumulator->bit_offset             = 0;
    accumulator->align_shift_right      = false;
    accumulator->state                  = HDLC_SYNC_STATE_HUNTING;
    accumulator->sync_byte              = sync_byte;
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
    while (scan_index < accumulator->position)
    {
        switch (accumulator->state)
        {
        case HDLC_SYNC_STATE_HUNTING:
        {
            size_t  start_index     = 0;
            uint8_t found_bit_pos   = 0;
            bool    found_shift_dir = false;

            bool found = hdlc_sync_find_complete_candidate_short(accumulator, scan_index,
                                                                 &start_index, &found_bit_pos,
                                                                 &found_shift_dir);
            if (!found)
            {
                found = hdlc_sync_find_opening_candidate(accumulator, scan_index, false,
                                                         &start_index, &found_bit_pos,
                                                         &found_shift_dir);
            }
            if (found)
            {
                accumulator->state             = HDLC_SYNC_STATE_SYNCING;
                accumulator->bit_offset        = found_bit_pos;
                accumulator->align_shift_right = found_shift_dir;
                accumulator->candidate_start   = start_index;
                accumulator->candidate_valid   = false;
                out_frame->length              = 1;
                out_frame->payload[0]          = accumulator->sync_byte;
                scan_index                     = start_index + 1;
                continue;
            }

            // Nothing found in current buffer window.
            scan_index = accumulator->position;
            break;
        }
        case HDLC_SYNC_STATE_SYNCING:
            if (out_frame->length >= HDLC_SYNC_MAX_REASONABLE_FRAME_SIZE_BYTES)
            {
                hdlc_sync_reject_oversized_candidate(accumulator, out_frame, &scan_index);
                break;
            }
            if (!hdlc_sync_get_aligned_byte(accumulator, scan_index, accumulator->bit_offset,
                                            accumulator->align_shift_right, &aligned))
            {
                if (accumulator->bit_offset != 0)
                {
                    accumulator->lookahead_wait_syncing++;
                }
                goto out;
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
            scan_index++;
            break;
        case HDLC_SYNC_STATE_SYNCED:
            if (out_frame->length >= HDLC_SYNC_MAX_REASONABLE_FRAME_SIZE_BYTES)
            {
                hdlc_sync_reject_oversized_candidate(accumulator, out_frame, &scan_index);
                break;
            }
            if (!hdlc_sync_get_aligned_byte(accumulator, scan_index, accumulator->bit_offset,
                                            accumulator->align_shift_right, &aligned))
            {
                if (accumulator->bit_offset != 0)
                {
                    accumulator->lookahead_wait_synced++;
                }
                goto out;
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
                accumulator->state           = HDLC_SYNC_STATE_HUNTING;
                accumulator->candidate_end   = scan_index + 1;
                accumulator->candidate_valid = true;

                // For diagnostics/tests we report the equivalent left-shift bit offset.
                if (accumulator->align_shift_right && accumulator->bit_offset > 0)
                {
                    accumulator->bit_offset = (uint8_t)(8U - accumulator->bit_offset);
                }

                result                        = E2S_ERR_HDLC_ACC_FRAME_READY;
                scan_index++;
                goto out;
            }
            scan_index++;
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
    if (accumulator->state == HDLC_SYNC_STATE_HUNTING)
    {
        // Keep one-byte overlap so a flag that starts at the previous last byte can
        // still be matched once the next byte arrives.
        size_t drop = (accumulator->processed > 0) ? (accumulator->processed - 1) : 0;
        if (drop > accumulator->position)
        {
            drop = accumulator->position;
        }
        hdlc_sync_drop_prefix(accumulator, drop);
        accumulator->processed = 0;
    }

    // In SYNCING/SYNCED we preserve the buffered candidate region and keep `processed`
    // as resume cursor. This allows trying alternate openings on candidate reject.
    return result;
}

void hdlc_sync_acc_consume_candidate(HDLC_SYNC_ACCUMULATOR_T* accumulator, bool accept)
{
    if (!accumulator || !accumulator->candidate_valid)
    {
        return;
    }

    size_t drop = 0;
    if (accept)
    {
        drop = accumulator->candidate_end;
    }
    else
    {
        // Keep alternate bit-phase candidates by advancing one raw byte from the start.
        drop = accumulator->candidate_start + 1;
    }
    if (drop > accumulator->position)
    {
        drop = accumulator->position;
    }
    if (drop > 0)
    {
        accumulator->consume_count++;
        hdlc_sync_drop_prefix(accumulator, drop);
    }
    hdlc_sync_reset_hunting_state(accumulator);

    // Hard cap: if buffer is near full, drop oldest bytes to keep bounded.
    if (accumulator->position >= (RX_HDLC_SYNC_MAX_BUFFER_SIZE - 16))
    {
        size_t keep = 16;
        size_t drop = accumulator->position - keep;
        accumulator->hardcap_drop_events++;
        accumulator->hardcap_drop_bytes += (uint32_t)drop;
        hdlc_sync_drop_prefix(accumulator, drop);
        accumulator->processed = 0;
    }
}
