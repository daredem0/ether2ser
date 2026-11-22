/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/protocol/hdlc_decoder.h
 * Purpose: HDLC decoder API.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef HDLC_DECODER_H
#define HDLC_DECODER_H

// Related headers

// Standard library headers
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Project Headers
#include "hdlc_common.h"

// Generated headers

/**
 * @brief Cumulative HDLC decode failure reason counters.
 */
typedef struct
{
    uint64_t invalid_frame;
    uint64_t too_short;
    uint64_t payload_too_long;
    uint64_t unstuff_error;
    uint64_t crc_mismatch;
} hdlc_decode_stats_t;

#define HDLC_DECODE_FAILURE_PREFIX_SIZE 12U

typedef enum
{
    HDLC_DECODE_FAILURE_NONE = 0,
    HDLC_DECODE_FAILURE_INVALID_FRAME,
    HDLC_DECODE_FAILURE_TOO_SHORT,
    HDLC_DECODE_FAILURE_PAYLOAD_TOO_LONG,
    HDLC_DECODE_FAILURE_UNSTUFF_ERROR,
    HDLC_DECODE_FAILURE_CRC_MISMATCH
} hdlc_decode_failure_reason_t;

/**
 * @brief First failed bit-stuffed decode captured since reset.
 */
typedef struct
{
    bool                         valid;
    hdlc_decode_failure_reason_t reason;
    size_t                       encoded_length;
    size_t                       raw_bit_index;
    size_t                       decoded_length;
    uint16_t                     received_crc;
    uint16_t                     calculated_crc;
    uint8_t                      prefix[HDLC_DECODE_FAILURE_PREFIX_SIZE];
    uint8_t                      prefix_length;
    uint8_t                      sync_bit_offset;
    bool                         align_shift_right;
} hdlc_decode_failure_diagnostic_t;

/**
 * @brief Decode an HDLC bit-stuffed frame.
 * @param frame Input HDLC frame.
 * @param payload Output payload buffer.
 * @param out_capacity Capacity of @p payload in bytes.
 * @param payload_length Output payload length.
 * @param lsb_first Bit order selector matching encoder/PIO path.
 * @param sync_bit_offset Synchronizer bit offset used for this candidate.
 * @param align_shift_right Synchronizer alignment direction used for this candidate.
 * @return true on successful decode and CRC validation.
 */
bool hdlc_decode(const HDLC_FRAME_T* frame, uint8_t* payload, const size_t out_capacity,
                 size_t* payload_length, bool lsb_first, uint8_t sync_bit_offset,
                 bool align_shift_right);

/**
 * @brief Decode a byte-escaped HDLC frame compatibility path.
 * @param frame Input HDLC frame.
 * @param payload Output payload buffer.
 * @param out_capacity Capacity of @p payload in bytes.
 * @param payload_length Output payload length.
 * @return true on successful decode and CRC validation.
 */
bool hdlc_decode_byte(const HDLC_FRAME_T* frame, uint8_t* payload, const size_t out_capacity,
                      size_t* payload_length);

/**
 * @brief Copy cumulative HDLC decode statistics.
 * @param out_stats Destination pointer.
 */
void hdlc_decode_stats_snapshot(hdlc_decode_stats_t* out_stats);

/**
 * @brief Reset cumulative HDLC decode statistics.
 */
void hdlc_decode_stats_reset(void);

/**
 * @brief Copy the first failed bit-stuffed decode captured since reset.
 * @param out_diagnostic Destination pointer.
 */
void hdlc_decode_failure_snapshot(hdlc_decode_failure_diagnostic_t* out_diagnostic);

/**
 * @brief Clear the retained first-failure decode diagnostic.
 */
void hdlc_decode_failure_reset(void);

#endif /* HDLC_DECODER_H */
