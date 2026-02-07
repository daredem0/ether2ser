
/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/protocol/hdlc_sync.h
 * Purpose: HDLC sync accumulator interface and constants.
 *
 * SPDX-License-Identifier: MIT
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

#define RX_HDLC_SYNC_MAX_BUFFER_SIZE 8192
#define HDLC_SYNC_DEFAULT_SYNC_BYTE HDLC_FLAG_BYTE
typedef enum
{
    HDLC_SYNC_STATE_HUNTING,
    HDLC_SYNC_STATE_SYNCING,
    HDLC_SYNC_STATE_SYNCED,
} HDLC_SYNC_STATE_T;

typedef struct
{
    uint8_t           buffer[RX_HDLC_SYNC_MAX_BUFFER_SIZE];
    size_t            position;
    size_t            processed;
    size_t            candidate_start;
    size_t            candidate_end;
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
} HDLC_SYNC_ACCUMULATOR_T;

void        hdlc_sync_acc_init(HDLC_SYNC_ACCUMULATOR_T* accumulator, uint8_t sync_byte);
bool        hdlc_sync_acc_process_byte(HDLC_SYNC_ACCUMULATOR_T* accumulator, uint8_t byte);
e2s_error_t hdlc_sync_acc_poll(HDLC_SYNC_ACCUMULATOR_T* accumulator, HDLC_FRAME_T* out_frame);
void        hdlc_sync_acc_consume_candidate(HDLC_SYNC_ACCUMULATOR_T* accumulator, bool accept);

#endif /* HDLC_SYNC_H */
