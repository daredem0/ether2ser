/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/protocol/hdlc_encoder.h
 * Purpose: HDLC encoder API.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef HDLC_ENCODER_H
#define HDLC_ENCODER_H

// Related headers

// Standard library headers
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Project Headers
#include "hdlc_common.h"

// Generated headers

/**
 * @brief Encode payload into an HDLC bit-stuffed frame.
 * @param payload Input payload bytes.
 * @param payload_length Number of payload bytes.
 * @param frame Output frame descriptor and storage.
 * @param lsb_first Bit order selector for bit-wise encoding.
 * @return true on success, false on invalid args or capacity overflow.
 */
bool hdlc_encode(const uint8_t* payload, const size_t payload_length, HDLC_FRAME_T* frame,
                 bool lsb_first);

/**
 * @brief Encode payload using byte-escaping HDLC compatibility path.
 * @param payload Input payload bytes.
 * @param payload_length Number of payload bytes.
 * @param frame Output frame descriptor and storage.
 * @return true on success, false on invalid args or capacity overflow.
 */
bool hdlc_encode_byte(const uint8_t* payload, const size_t payload_length, HDLC_FRAME_T* frame);

#endif /* HDLC_ENCODER_H */
