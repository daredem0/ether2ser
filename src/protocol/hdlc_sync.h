
/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/protocol/hdlc_sync.h
 * Purpose: HDLC sync accumulator interface and constants.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef HDLC_SYNC_H
#define HDLC_SYNC_H

// Related headers

// Standard library headers
#include <stdbool.h>

// Project Headers
#include "hdlc_common.h"
#include "system/error.h"

// Generated headers

/**
 * @brief Maximum raw RX bytes retained in the HDLC sync accumulator.
 */
#define RX_HDLC_SYNC_MAX_BUFFER_SIZE 8192

/**
 * @brief Default sync byte used for HDLC framing.
 */
#define HDLC_SYNC_DEFAULT_SYNC_BYTE HDLC_FLAG_BYTE

/**
 * @brief HDLC synchronizer state machine states.
 */
typedef enum
{
    /** Searching for first sync/flag pattern. */
    HDLC_SYNC_STATE_HUNTING,
    /** Sync found, building candidate with known bit offset. */
    HDLC_SYNC_STATE_SYNCING,
    /** Actively consuming aligned bytes until closing flag. */
    HDLC_SYNC_STATE_SYNCED,
} HDLC_SYNC_STATE_T;

/**
 * @brief Accumulator and state for HDLC bit-offset synchronization.
 */
typedef struct
{
    /** Raw incoming byte buffer. */
    uint8_t           buffer[RX_HDLC_SYNC_MAX_BUFFER_SIZE];
    /** Number of bytes currently stored in @ref buffer. */
    size_t            position;
    /** Number of bytes already processed by poll state machine. */
    size_t            processed;
    /** Candidate frame start index. */
    size_t            candidate_start;
    /** Candidate frame end index (exclusive). */
    size_t            candidate_end;
    /** Whether a valid candidate is currently available. */
    bool              candidate_valid;
    size_t            candidate_i;
    uint8_t           candidate_bit_pos;
    bool              resume_pending;
    size_t            resume_i;
    uint8_t           resume_bit_pos;
    uint8_t           bit_offset;
    HDLC_SYNC_STATE_T state;
    uint8_t           sync_byte;
    uint16_t          sync_accumulator;

    uint32_t lookahead_wait_syncing;
    uint32_t lookahead_wait_synced;
    uint32_t frame_ready_count;
    uint32_t consume_count;
    uint32_t hardcap_drop_events;
    uint32_t hardcap_drop_bytes;
} HDLC_SYNC_ACCUMULATOR_T;

/**
 * @brief Initialize HDLC sync accumulator state.
 * @param accumulator Accumulator instance.
 * @param sync_byte Sync/flag byte to detect.
 */
void        hdlc_sync_acc_init(HDLC_SYNC_ACCUMULATOR_T* accumulator, uint8_t sync_byte);

/**
 * @brief Append one received raw byte to the accumulator.
 * @param accumulator Accumulator instance.
 * @param byte Received raw byte.
 * @return true if appended, false if buffer full or invalid args.
 */
bool        hdlc_sync_acc_process_byte(HDLC_SYNC_ACCUMULATOR_T* accumulator, uint8_t byte);

/**
 * @brief Poll accumulator for an aligned HDLC frame candidate.
 * @param accumulator Accumulator instance.
 * @param out_frame Output aligned frame buffer.
 * @return `E2S_ERR_HDLC_ACC_FRAME_READY` when a candidate is available, otherwise status code.
 */
e2s_error_t hdlc_sync_acc_poll(HDLC_SYNC_ACCUMULATOR_T* accumulator, HDLC_FRAME_T* out_frame);

/**
 * @brief Consume current candidate and advance accumulator window.
 * @param accumulator Accumulator instance.
 * @param accept Acceptance hint for candidate handling.
 */
void        hdlc_sync_acc_consume_candidate(HDLC_SYNC_ACCUMULATOR_T* accumulator, bool accept);

#endif /* HDLC_SYNC_H */
