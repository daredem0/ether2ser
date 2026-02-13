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
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Project Headers
#include "hdlc_common.h"
#include "system/error.h"

// Generated headers

/**
 * @brief Smallest valid encoded HDLC frame: opening flag + FCS + closing flag.
 */
#define HDLC_SYNC_MIN_FRAME_SIZE_BYTES 4U

static void hdlc_sync_drop_prefix(HDLC_SYNC_ACCUMULATOR_T* accumulator, size_t drop_count)
{
    if (!accumulator || drop_count == 0U)
    {
        return;
    }
    if (drop_count >= accumulator->position)
    {
        accumulator->position  = 0U;
        accumulator->processed = 0U;
        return;
    }

    size_t remaining = accumulator->position - drop_count;
    memmove(accumulator->buffer, accumulator->buffer + drop_count, remaining);
    accumulator->position = remaining;
}

static bool hdlc_sync_get_aligned_byte(const HDLC_SYNC_ACCUMULATOR_T* accumulator, size_t raw_index,
                                       uint8_t bit_offset, bool shift_right, uint8_t* out_byte)
{
    if (!accumulator || !out_byte || raw_index >= accumulator->position || bit_offset >= CHAR_BIT)
    {
        return false;
    }

    if (bit_offset == 0U)
    {
        *out_byte = accumulator->buffer[raw_index];
        return true;
    }

    if ((raw_index + 1U) >= accumulator->position)
    {
        return false;
    }

    if (shift_right)
    {
        *out_byte = (uint8_t)((uint8_t)(accumulator->buffer[raw_index] >> bit_offset) |
                              (uint8_t)(accumulator->buffer[raw_index + 1U] <<
                                        (CHAR_BIT - bit_offset)));
    }
    else
    {
        *out_byte = (uint8_t)((uint8_t)(accumulator->buffer[raw_index] << bit_offset) |
                              (uint8_t)(accumulator->buffer[raw_index + 1U] >>
                                        (CHAR_BIT - bit_offset)));
    }
    return true;
}

static bool hdlc_sync_find_opening_flag(HDLC_SYNC_ACCUMULATOR_T* accumulator, size_t* raw_index,
                                        uint8_t* bit_offset, bool* shift_right)
{
    if (!accumulator || !raw_index || !bit_offset || !shift_right)
    {
        return false;
    }

    size_t scan_index = accumulator->processed;
    if (scan_index >= accumulator->position)
    {
        scan_index = 0U;
    }

    bool   pending_valid     = false;
    size_t pending_raw_index = 0U;

    // Search for the earliest opening flag that already has a closing flag in current buffer.
    while (scan_index < accumulator->position)
    {
        for (uint8_t offset = 0U; offset < CHAR_BIT; ++offset)
        {
            for (uint8_t dir_index = 0U; dir_index < 2U; ++dir_index)
            {
                bool shift_right_mode = (dir_index == 1U);
                if (offset == 0U && shift_right_mode)
                {
                    continue; // off=0 has only one effective direction.
                }

                uint8_t candidate = 0U;
                if (!hdlc_sync_get_aligned_byte(accumulator, scan_index, offset, shift_right_mode,
                                                &candidate))
                {
                    // Need more data for this alignment at this raw index.
                    continue;
                }
                if (candidate != accumulator->sync_byte)
                {
                    continue;
                }

                size_t probe_index = scan_index + 1U;
                size_t frame_bytes = 1U;
                bool   complete    = false;
                while (probe_index < accumulator->position)
                {
                    uint8_t probe_aligned = 0U;
                    if (!hdlc_sync_get_aligned_byte(accumulator, probe_index, offset,
                                                    shift_right_mode, &probe_aligned))
                    {
                        break;
                    }
                    frame_bytes++;
                    if (probe_aligned == accumulator->sync_byte &&
                        frame_bytes >= HDLC_SYNC_MIN_FRAME_SIZE_BYTES)
                    {
                        complete = true;
                        break;
                    }
                    probe_index++;
                }

                if (complete)
                {
                    *raw_index   = scan_index;
                    *bit_offset  = offset;
                    *shift_right = shift_right_mode;
                    return true;
                }

                // Keep earliest incomplete opening as fallback if no complete frame in buffer yet.
                if (!pending_valid)
                {
                    pending_valid     = true;
                    pending_raw_index = scan_index;
                }
            }
        }
        scan_index++;
    }

    if (pending_valid)
    {
        // Keep data from earliest plausible opening flag and wait for more bytes.
        accumulator->processed = pending_raw_index;
    }
    else
    {
        // No opening found: keep one-byte overlap for next call.
        accumulator->processed = (accumulator->position > 0U) ? (accumulator->position - 1U) : 0U;
    }
    return false;
}

void hdlc_sync_acc_init(HDLC_SYNC_ACCUMULATOR_T* accumulator, uint8_t sync_byte)
{
    accumulator->position               = 0U;
    accumulator->processed              = 0U;
    accumulator->candidate_start        = 0U;
    accumulator->candidate_end          = 0U;
    accumulator->candidate_valid        = false;
    accumulator->candidate_i            = 0U;
    accumulator->candidate_bit_pos      = 0U;
    accumulator->resume_pending         = false;
    accumulator->resume_i               = 0U;
    accumulator->resume_bit_pos         = 0U;
    accumulator->bit_offset             = 0U;
    accumulator->align_shift_right      = false;
    accumulator->state                  = HDLC_SYNC_STATE_HUNTING;
    accumulator->sync_byte              = sync_byte;
    accumulator->sync_accumulator       = 0U;
    accumulator->lookahead_wait_syncing = 0U;
    accumulator->lookahead_wait_synced  = 0U;
    accumulator->frame_ready_count      = 0U;
    accumulator->consume_count          = 0U;
    accumulator->hardcap_drop_events    = 0U;
    accumulator->hardcap_drop_bytes     = 0U;
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
    if (!accumulator || !out_frame || !out_frame->payload || out_frame->capacity == 0U)
    {
        return E2S_OK;
    }
    if (accumulator->position == 0U)
    {
        return E2S_OK;
    }

    if (accumulator->state == HDLC_SYNC_STATE_HUNTING)
    {
        size_t  start_raw_index  = 0U;
        uint8_t start_bit_offset = 0U;
        bool    start_shift_right = false;
        if (!hdlc_sync_find_opening_flag(accumulator, &start_raw_index, &start_bit_offset,
                                         &start_shift_right))
        {
            // Keep the first unscanned byte as overlap for next hunt pass.
            hdlc_sync_drop_prefix(accumulator, accumulator->processed);
            accumulator->processed = 0U;
            return E2S_OK;
        }

        accumulator->state             = HDLC_SYNC_STATE_SYNCING;
        accumulator->bit_offset        = start_bit_offset;
        accumulator->align_shift_right = start_shift_right;
        accumulator->candidate_start   = start_raw_index;
        accumulator->candidate_i       = start_raw_index;
        accumulator->candidate_bit_pos = start_bit_offset;
        accumulator->candidate_valid   = false;
        accumulator->sync_accumulator  = 0U;

        out_frame->length            = 1U;
        out_frame->payload[0]        = accumulator->sync_byte;
        accumulator->processed        = start_raw_index + 1U;
    }
    while (accumulator->state == HDLC_SYNC_STATE_SYNCING ||
           accumulator->state == HDLC_SYNC_STATE_SYNCED)
    {
        if (accumulator->state == HDLC_SYNC_STATE_SYNCING)
        {
            uint8_t aligned = 0U;
            if (!hdlc_sync_get_aligned_byte(accumulator, accumulator->processed,
                                            accumulator->bit_offset, accumulator->align_shift_right,
                                            &aligned))
            {
                if (accumulator->bit_offset != 0U)
                {
                    accumulator->lookahead_wait_syncing++;
                }
                // Keep from first unconsumed aligned raw index onward.
                hdlc_sync_drop_prefix(accumulator, accumulator->processed);
                accumulator->processed = 0U;
                return E2S_OK;
            }

            if (out_frame->length >= out_frame->capacity)
            {
                out_frame->length       = 0U;
                accumulator->state      = HDLC_SYNC_STATE_HUNTING;
                accumulator->bit_offset = 0U;
                return E2S_ERR_HDLC_DECODE_PAYLOAD_TOO_LONG;
            }

            out_frame->payload[out_frame->length++] = aligned;
            accumulator->processed++;
            accumulator->state = HDLC_SYNC_STATE_SYNCED;
            continue;
        }

        uint8_t aligned = 0U;
        if (!hdlc_sync_get_aligned_byte(accumulator, accumulator->processed, accumulator->bit_offset,
                                        accumulator->align_shift_right, &aligned))
        {
            if (accumulator->bit_offset != 0U)
            {
                accumulator->lookahead_wait_synced++;
            }
            // Keep from first unconsumed aligned raw index onward.
            hdlc_sync_drop_prefix(accumulator, accumulator->processed);
            accumulator->processed = 0U;
            return E2S_OK;
        }

        if (out_frame->length >= out_frame->capacity)
        {
            out_frame->length       = 0U;
            accumulator->state      = HDLC_SYNC_STATE_HUNTING;
            accumulator->bit_offset = 0U;
            return E2S_ERR_HDLC_DECODE_PAYLOAD_TOO_LONG;
        }

        out_frame->payload[out_frame->length++] = aligned;
        accumulator->processed++;

        if (aligned == accumulator->sync_byte)
        {
            if (out_frame->length < HDLC_SYNC_MIN_FRAME_SIZE_BYTES)
            {
                // Repeated flags: keep this as new opening flag and continue.
                out_frame->length     = 1U;
                out_frame->payload[0] = accumulator->sync_byte;
                continue;
            }

            accumulator->frame_ready_count++;
            accumulator->state            = HDLC_SYNC_STATE_HUNTING;
            accumulator->sync_accumulator = 0U;
            accumulator->candidate_end    = accumulator->processed;
            accumulator->candidate_valid  = true;
            accumulator->processed        = 0U;
            return E2S_ERR_HDLC_ACC_FRAME_READY;
        }
    }

    return E2S_OK;
}

void hdlc_sync_acc_consume_candidate(HDLC_SYNC_ACCUMULATOR_T* accumulator, bool accept)
{
    if (!accumulator || !accumulator->candidate_valid)
    {
        return;
    }

    size_t drop = 0U;
    if (accept)
    {
        // Accepted candidate: advance beyond the full frame boundary.
        drop = accumulator->candidate_end;
    }
    else
    {
        // Rejected candidate: only advance one raw byte past the opening flag start
        // so alternate bit-phase candidates still available in this region are kept.
        drop = accumulator->candidate_start + 1U;
    }

    if (drop > accumulator->position)
    {
        drop = accumulator->position;
    }
    if (drop > 0U)
    {
        accumulator->consume_count++;
        hdlc_sync_drop_prefix(accumulator, drop);
    }

    accumulator->processed         = 0U;
    accumulator->candidate_start   = 0U;
    accumulator->candidate_end     = 0U;
    accumulator->candidate_valid   = false;
    accumulator->candidate_i       = 0U;
    accumulator->candidate_bit_pos = 0U;
    accumulator->bit_offset        = 0U;
    accumulator->align_shift_right = false;
    accumulator->state             = HDLC_SYNC_STATE_HUNTING;
    accumulator->sync_accumulator  = 0U;
    accumulator->resume_pending    = false;
    accumulator->resume_i          = 0U;
    accumulator->resume_bit_pos    = 0U;

    // Hard cap: if buffer is near full, drop oldest bytes to keep bounded.
    if (accumulator->position >= (RX_HDLC_SYNC_MAX_BUFFER_SIZE - 16U))
    {
        size_t keep = 16U;
        size_t drop = accumulator->position - keep;
        accumulator->hardcap_drop_events++;
        accumulator->hardcap_drop_bytes += (uint32_t)drop;
        hdlc_sync_drop_prefix(accumulator, drop);
        accumulator->processed      = 0U;
        accumulator->resume_pending = false;
    }
}
