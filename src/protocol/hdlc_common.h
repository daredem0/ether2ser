/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/protocol/hdlc_common.h
 * Purpose: HDLC common utilities API (CRC and shared helpers).
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef HDLC_COMMON_H
#define HDLC_COMMON_H

// Related headers

// Standard library headers
#include <stddef.h>
#include <stdint.h>

// Project Headers

// Generated headers

/**
 * @name HDLC Constants
 * @{
 */
#define HDLC_FLAG_BYTE 0x7E
#define HDLC_ESCAPE_BYTE 0x7D
#define HDLC_ESCAPE_XOR 0x20
/** @} */

/**
 * @brief Generic HDLC frame buffer descriptor.
 */
typedef struct
{
    /** Buffer containing encoded or decoded frame data. */
    uint8_t* payload;
    /** Current number of valid bytes in @ref payload. */
    size_t   length;
    /** Maximum number of bytes writable to @ref payload. */
    size_t   capacity;
} HDLC_FRAME_T;

/**
 * @brief Compute HDLC CRC16 (FCS) over a payload.
 * @param payload Input data bytes.
 * @param num_bytes Number of bytes in @p payload.
 * @return Computed CRC16 value.
 */
uint16_t hdlc_crc16(const uint8_t* payload, size_t num_bytes);

#endif /* HDLC_COMMON_H */
