/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/protocol/hdlc_encoder.h
 * Purpose: HDLC encoder API.
 *
 * SPDX-License-Identifier: MIT
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

bool hdlc_encode(const uint8_t* payload, const size_t payload_length, HDLC_FRAME_T* frame);
bool hdlc_encode_byte(const uint8_t* payload, const size_t payload_length, HDLC_FRAME_T* frame);

#endif /* HDLC_ENCODER_H */
