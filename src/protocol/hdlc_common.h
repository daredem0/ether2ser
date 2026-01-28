/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/protocol/hdlc_common.h
 * Purpose: HDLC common utilities API (CRC and shared helpers).
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef HDLC_COMMON_H
#define HDLC_COMMON_H

#include <stdint.h>
#include <stddef.h>

// HDLC protocol constants
#define HDLC_FLAG_BYTE 0x7E
#define HDLC_ESCAPE_BYTE 0x7D
#define HDLC_ESCAPE_XOR 0x20

// HDLC Frame Structure
typedef struct{
    uint8_t *payload;    // Buffer containing encoded frame
    size_t length;      // Length of encoded frame
    size_t capacity;    // Maximum length of encoded frame
} HDLC_FRAME_T;

uint16_t hdlc_crc16(const uint8_t* payload, size_t num_bytes);
#endif /* HDLC_COMMON_H */
