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

#define HDLC_BITS_PER_BYTE 8U
#define HDLC_SYNC_MIN_FRAME_SIZE_BYTES 4U
#define HDLC_SYNC_MAX_REASONABLE_FRAME_SIZE_BYTES 2048U
#define HDLC_SYNC_SHORT_BUFFER_SEARCH_LIMIT 64U

/*
 * Module model:
 * - Input is a continuous raw-byte stream from PIO where HDLC flag alignment is unknown.
 * - We detect an opening flag (possibly bit-shifted), then keep consuming aligned bytes
 *   until we hit a closing flag.
 * - We return FRAME_READY with the still-encoded frame (flags + stuffed bytes + FCS).
 * - Caller decides accept/reject after decode/CRC and then calls consume_candidate().
 *
 * State machine (poll side):
 *
 *   HUNTING
 *      |  found opening flag + bit phase
 *      v
 *   SYNCING  -- consume first aligned byte -->  SYNCED
 *      ^                                         |
 *      |<----------- closing flag -------------- |
 *
 *   From SYNCING/SYNCED:
 *   - if more raw data needed: return E2S_OK and resume next poll() from `processed`
 *   - if candidate ready: return E2S_ERR_HDLC_ACC_FRAME_READY and wait for consume_candidate()
 *
 * Ownership split:
 * - poll() discovers candidates and exposes them.
 * - consume_candidate() advances raw buffer based on decode accept/reject decision.
 */

typedef enum
{
    // Consumed at least one aligned byte and can continue in the same poll call.
    HDLC_SYNC_POLL_STEP_PROGRESS,
    // Need more raw data before we can continue.
    HDLC_SYNC_POLL_STEP_STOP,
    // Found full candidate bounded by closing flag.
    HDLC_SYNC_POLL_STEP_FRAME_READY,
    // Fatal for this poll call (e.g. destination frame buffer too small).
    HDLC_SYNC_POLL_STEP_ERROR,
} HDLC_SYNC_POLL_STEP_RESULT_T;

// Drop oldest raw bytes while keeping relative order of the remainder.
static void hdlc_sync_drop_prefix(HDLC_SYNC_ACCUMULATOR_T* accumulator, size_t drop_count)
{
    if (!accumulator || drop_count == 0)
    {
        return;
    }

    if (drop_count >= accumulator->position)
    {
        // Everything is consumed; cursor must be reset as well.
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

    // Reset to "no active candidate" baseline.
    accumulator->processed         = 0;
    accumulator->candidate_start   = 0;
    accumulator->candidate_end     = 0;
    accumulator->candidate_valid   = false;
    accumulator->state             = HDLC_SYNC_STATE_HUNTING;
    accumulator->bit_offset        = 0;
    accumulator->align_shift_right = false;
}

// False-lock recovery when candidate grows implausibly large.
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

/*
 * Reconstruct one aligned byte from raw stream at the selected bit phase.
 *
 * shift_right = true  -> byte is composed as raw[i] >> off | raw[i+1] << (8-off)
 * shift_right = false -> byte is composed as raw[i] << off | raw[i+1] >> (8-off)
 */
static bool hdlc_sync_get_aligned_byte(const HDLC_SYNC_ACCUMULATOR_T* accumulator, size_t raw_index,
                                       uint8_t bit_offset, bool shift_right, uint8_t* out_byte)
{
    if (!accumulator || !out_byte || raw_index >= accumulator->position ||
        bit_offset >= HDLC_BITS_PER_BYTE)
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
                              (uint8_t)(accumulator->buffer[raw_index + 1] <<
                                        (HDLC_BITS_PER_BYTE - bit_offset)));
    }
    else
    {
        *out_byte = (uint8_t)((uint8_t)(accumulator->buffer[raw_index] << bit_offset) |
                              (uint8_t)(accumulator->buffer[raw_index + 1] >>
                                        (HDLC_BITS_PER_BYTE - bit_offset)));
    }
    return true;
}

/*
 * Fast opening-flag hunt:
 * - byte-aligned first
 * - then right-shift phases (most common in our traces)
 * - optional left-shift fallback if caller enables it
 */
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

    // Start one raw byte earlier so a boundary-spanning flag is not missed.
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
        for (uint8_t bit_pos = 1; bit_pos < HDLC_BITS_PER_BYTE; ++bit_pos)
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
            for (uint8_t bit_pos = 1; bit_pos < HDLC_BITS_PER_BYTE; ++bit_pos)
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

/*
 * Exhaustive search used only for short buffers:
 * for each possible alignment, find complete flag...flag candidate and keep the longest.
 *
 * Why longest:
 * short/noisy buffers can contain multiple false openings; longest span tends to represent
 * the real frame candidate when close/overlapping flag signatures appear.
 */
static bool hdlc_sync_find_complete_candidate_short(const HDLC_SYNC_ACCUMULATOR_T* accumulator,
                                                    size_t scan_index, size_t* out_start_index,
                                                    uint8_t* out_bit_pos, bool* out_shift_right)
{
    if (!accumulator || !out_start_index || !out_bit_pos || !out_shift_right ||
        accumulator->position < HDLC_SYNC_MIN_FRAME_SIZE_BYTES ||
        accumulator->position > HDLC_SYNC_SHORT_BUFFER_SEARCH_LIMIT)
    {
        // For long buffers we intentionally skip this expensive exhaustive path.
        return false;
    }

    // Start one byte earlier to keep boundary-spanning openings visible.
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
        // Try all bit phases for this raw start index.
        for (uint8_t bit_pos = 0; bit_pos < HDLC_BITS_PER_BYTE; ++bit_pos)
        {
            // mode=0 -> left-shift composition, mode=1 -> right-shift composition.
            for (uint8_t mode = 0; mode < 2; ++mode)
            {
                bool shift_right = (mode != 0);
                if (bit_pos == 0 && shift_right)
                {
                    // offset=0 has no direction; avoid duplicate test.
                    continue;
                }

                if (!hdlc_sync_get_aligned_byte(accumulator, start, bit_pos, shift_right,
                                                &start_byte) ||
                    start_byte != accumulator->sync_byte)
                {
                    // This alignment does not start with a flag; skip quickly.
                    continue;
                }

                // We have a plausible opening flag: probe forward for matching closing flag.
                size_t frame_size = 1;
                for (size_t probe = start + 1; probe < accumulator->position; ++probe)
                {
                    if (!hdlc_sync_get_aligned_byte(accumulator, probe, bit_pos, shift_right,
                                                    &probe_byte))
                    {
                        // Need one more raw byte for this alignment before continuing probe.
                        break;
                    }
                    frame_size++;
                    if (probe_byte == accumulator->sync_byte &&
                        frame_size >= HDLC_SYNC_MIN_FRAME_SIZE_BYTES)
                    {
                        // Require at least: opening flag + 2-byte FCS + closing flag.
                        if (!found_best || frame_size > best_frame_size)
                        {
                            // Keep longest complete candidate found in this short window.
                            found_best      = true;
                            best_start      = start;
                            best_bit_pos    = bit_pos;
                            best_shift      = shift_right;
                            best_frame_size = frame_size;
                        }
                        // This opening is already closed; no need to probe further.
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
    // Caller enters SYNCING with this chosen opening+alignment.
    return true;
}

// HUNTING -> SYNCING transition (select opening flag + alignment).
static HDLC_SYNC_POLL_STEP_RESULT_T hdlc_sync_step_hunting(HDLC_SYNC_ACCUMULATOR_T* accumulator,
                                                           HDLC_FRAME_T* out_frame,
                                                           size_t* scan_index)
{
    size_t  start_index     = 0;
    uint8_t found_bit_pos   = 0;
    bool    found_shift_dir = false;

    bool found = hdlc_sync_find_complete_candidate_short(
        accumulator, *scan_index, &start_index, &found_bit_pos, &found_shift_dir);
    if (!found)
    {
        // For longer buffers, fast opening search is cheaper than exhaustive pairing.
        found = hdlc_sync_find_opening_candidate(accumulator, *scan_index, false, &start_index,
                                                 &found_bit_pos, &found_shift_dir);
    }

    if (!found)
    {
        // Nothing found in current buffer window.
        *scan_index = accumulator->position;
        return HDLC_SYNC_POLL_STEP_STOP;
    }

    accumulator->state             = HDLC_SYNC_STATE_SYNCING;
    accumulator->bit_offset        = found_bit_pos;
    accumulator->align_shift_right = found_shift_dir;
    accumulator->candidate_start   = start_index;
    accumulator->candidate_valid   = false;
    out_frame->length              = 1;
    out_frame->payload[0]          = accumulator->sync_byte;
    *scan_index                    = start_index + 1;
    return HDLC_SYNC_POLL_STEP_PROGRESS;
}

// Consume the first aligned byte after opening flag.
static HDLC_SYNC_POLL_STEP_RESULT_T hdlc_sync_step_syncing(HDLC_SYNC_ACCUMULATOR_T* accumulator,
                                                           HDLC_FRAME_T* out_frame,
                                                           size_t* scan_index,
                                                           e2s_error_t* out_error)
{
    if (out_frame->length >= HDLC_SYNC_MAX_REASONABLE_FRAME_SIZE_BYTES)
    {
        // Guardrail against false lock on idle/noise runs.
        hdlc_sync_reject_oversized_candidate(accumulator, out_frame, scan_index);
        return HDLC_SYNC_POLL_STEP_PROGRESS;
    }

    uint8_t aligned = 0;
    if (!hdlc_sync_get_aligned_byte(accumulator, *scan_index, accumulator->bit_offset,
                                    accumulator->align_shift_right, &aligned))
    {
        if (accumulator->bit_offset != 0)
        {
            // Non-zero offset needs raw[i+1] lookahead; count those wait events.
            accumulator->lookahead_wait_syncing++;
        }
        return HDLC_SYNC_POLL_STEP_STOP;
    }

    if (out_frame->length >= out_frame->capacity)
    {
        // Destination frame buffer is owned by caller; fail hard to avoid overwrite.
        out_frame->length       = 0;
        accumulator->state      = HDLC_SYNC_STATE_HUNTING;
        accumulator->bit_offset = 0;
        *out_error              = E2S_ERR_HDLC_DECODE_PAYLOAD_TOO_LONG;
        return HDLC_SYNC_POLL_STEP_ERROR;
    }

    out_frame->payload[out_frame->length++] = aligned;
    accumulator->state                      = HDLC_SYNC_STATE_SYNCED;
    (*scan_index)++;
    return HDLC_SYNC_POLL_STEP_PROGRESS;
}

// Consume aligned bytes until closing flag; then report candidate ready.
static HDLC_SYNC_POLL_STEP_RESULT_T hdlc_sync_step_synced(HDLC_SYNC_ACCUMULATOR_T* accumulator,
                                                          HDLC_FRAME_T* out_frame,
                                                          size_t* scan_index,
                                                          e2s_error_t* out_error)
{
    if (out_frame->length >= HDLC_SYNC_MAX_REASONABLE_FRAME_SIZE_BYTES)
    {
        // Guardrail against false lock on idle/noise runs.
        hdlc_sync_reject_oversized_candidate(accumulator, out_frame, scan_index);
        return HDLC_SYNC_POLL_STEP_PROGRESS;
    }

    uint8_t aligned = 0;
    if (!hdlc_sync_get_aligned_byte(accumulator, *scan_index, accumulator->bit_offset,
                                    accumulator->align_shift_right, &aligned))
    {
        if (accumulator->bit_offset != 0)
        {
            // Non-zero offset needs raw[i+1] lookahead; count those wait events.
            accumulator->lookahead_wait_synced++;
        }
        return HDLC_SYNC_POLL_STEP_STOP;
    }

    if (out_frame->length >= out_frame->capacity)
    {
        // Destination frame buffer is owned by caller; fail hard to avoid overwrite.
        out_frame->length       = 0;
        accumulator->state      = HDLC_SYNC_STATE_HUNTING;
        accumulator->bit_offset = 0;
        *out_error              = E2S_ERR_HDLC_DECODE_PAYLOAD_TOO_LONG;
        return HDLC_SYNC_POLL_STEP_ERROR;
    }

    out_frame->payload[out_frame->length++] = aligned;
    if (aligned != accumulator->sync_byte)
    {
        // Still inside payload/FCS.
        (*scan_index)++;
        return HDLC_SYNC_POLL_STEP_PROGRESS;
    }

    // Closing flag reached: candidate is complete in raw buffer.
    accumulator->frame_ready_count++;
    accumulator->state           = HDLC_SYNC_STATE_HUNTING;
    accumulator->candidate_end   = *scan_index + 1;
    accumulator->candidate_valid = true;

    // For diagnostics/tests we report the equivalent left-shift bit offset.
    if (accumulator->align_shift_right && accumulator->bit_offset > 0)
    {
        accumulator->bit_offset = (uint8_t)(HDLC_BITS_PER_BYTE - accumulator->bit_offset);
    }

    (*scan_index)++;
    return HDLC_SYNC_POLL_STEP_FRAME_READY;
}

/*
 * Finalize poll() when no frame was produced:
 * - in HUNTING: drop scanned prefix but keep 1-byte overlap so split flags survive
 * - in SYNCING/SYNCED: keep buffered candidate and processed cursor for continuation
 */
static void hdlc_sync_finalize_poll_no_frame(HDLC_SYNC_ACCUMULATOR_T* accumulator, size_t scan_index)
{
    if (!accumulator)
    {
        return;
    }

    accumulator->processed = scan_index;
    if (accumulator->state != HDLC_SYNC_STATE_HUNTING)
    {
        return;
    }

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

void hdlc_sync_acc_init(HDLC_SYNC_ACCUMULATOR_T* accumulator, uint8_t sync_byte)
{
    // Full explicit init is intentional: structure may be stack-allocated.
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
        // Backpressure is handled by caller; here we simply refuse new byte.
        return false;
    }
    accumulator->buffer[accumulator->position] = byte;
    accumulator->position++;
    return true;
}

/*
 * Poll contract:
 * - returns E2S_OK when no complete candidate available yet
 * - returns E2S_ERR_HDLC_ACC_FRAME_READY when out_frame holds one candidate
 * - caller must then call hdlc_sync_acc_consume_candidate(accept)
 */
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

    // Only needed when aligned byte assembly needs lookahead.
    if (accumulator->state != HDLC_SYNC_STATE_HUNTING && accumulator->bit_offset != 0 &&
        accumulator->position < 2)
    {
        // Need one more raw byte for cross-byte reconstruction.
        return E2S_OK;
    }

    // `scan_index` is the raw cursor local to this poll cycle.
    size_t scan_index = accumulator->processed;
    while (scan_index < accumulator->position)
    {
        HDLC_SYNC_POLL_STEP_RESULT_T step_result = HDLC_SYNC_POLL_STEP_STOP;
        e2s_error_t                  error_code  = E2S_OK;

        switch (accumulator->state)
        {
        case HDLC_SYNC_STATE_HUNTING:
            // Select candidate opening + lock alignment.
            step_result = hdlc_sync_step_hunting(accumulator, out_frame, &scan_index);
            break;
        case HDLC_SYNC_STATE_SYNCING:
            // Consume first data byte after opening flag.
            step_result =
                hdlc_sync_step_syncing(accumulator, out_frame, &scan_index, &error_code);
            break;
        case HDLC_SYNC_STATE_SYNCED:
            // Continue until closing flag.
            step_result = hdlc_sync_step_synced(accumulator, out_frame, &scan_index, &error_code);
            break;
        default:
            return E2S_OK;
        }

        if (step_result == HDLC_SYNC_POLL_STEP_PROGRESS)
        {
            continue;
        }

        if (step_result == HDLC_SYNC_POLL_STEP_FRAME_READY)
        {
            // Candidate lifecycle switches to caller; next poll starts fresh.
            accumulator->processed = 0;
            return E2S_ERR_HDLC_ACC_FRAME_READY;
        }

        if (step_result == HDLC_SYNC_POLL_STEP_ERROR)
        {
            return error_code;
        }

        // STOP: need more raw bytes or no useful work left in this poll cycle.
        break;
    }

    hdlc_sync_finalize_poll_no_frame(accumulator, scan_index);

    // In SYNCING/SYNCED we preserve the buffered candidate region and keep `processed`
    // as resume cursor. This allows trying alternate openings on candidate reject.
    return E2S_OK;
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
        // Accepted frame: drop full [opening..closing] candidate region.
        drop = accumulator->candidate_end;
    }
    else
    {
        // Reject strategy: advance only one raw byte from opening.
        // This preserves overlapping candidates at other bit phases.
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
        drop        = accumulator->position - keep;
        accumulator->hardcap_drop_events++;
        accumulator->hardcap_drop_bytes += (uint32_t)drop;
        // Keep a small tail so boundary-spanning flags still have a chance.
        hdlc_sync_drop_prefix(accumulator, drop);
        accumulator->processed = 0;
    }
}
