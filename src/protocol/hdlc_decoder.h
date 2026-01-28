/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/protocol/hdlc_decoder.h
 * Purpose: HDLC decoder API.
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */


#ifndef HDLC_DECODER_H
#define HDLC_DECODER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "hdlc_common.h"

bool hdlc_decode(const HDLC_FRAME_T *frame, uint8_t *payload, const size_t out_capacity, size_t *payload_length);

#endif /* HDLC_DECODER_H */
