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
 * @brief Decode an HDLC bit-stuffed frame.
 * @param frame Input HDLC frame.
 * @param payload Output payload buffer.
 * @param out_capacity Capacity of @p payload in bytes.
 * @param payload_length Output payload length.
 * @param lsb_first Bit order selector matching encoder/PIO path.
 * @return true on successful decode and CRC validation.
 */
bool hdlc_decode(const HDLC_FRAME_T* frame, uint8_t* payload, const size_t out_capacity,
                 size_t* payload_length, bool lsb_first);

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

#endif /* HDLC_DECODER_H */
