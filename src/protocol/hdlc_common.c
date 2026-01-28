/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/protocol/hdlc_common.c
 * Purpose: HDLC common utilities (CRC and shared helpers).
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "hdlc_common.h"

// Standard library headers
#include <stddef.h>
#include <stdint.h>

// Library Headers

// Project Headers

// Generated headers

#define HDLC_CRC16_CCITT_INIT     0xFFFFu
#define HDLC_CRC16_CCITT_POLY     0x1021u
#define HDLC_CRC16_CCITT_MSB_MASK 0x8000u
#define HDLC_CRC16_BITS_PER_BYTE  8u

static uint16_t crc16_ccitt_false(const uint8_t* payload, size_t num_bytes){
    uint8_t bit;
    uint16_t crc16 = HDLC_CRC16_CCITT_INIT;
    while (num_bytes--) {
        uint8_t byte = *payload++;
        crc16 ^= (uint16_t)byte << HDLC_CRC16_BITS_PER_BYTE;
        for (uint8_t bit = 0; bit < HDLC_CRC16_BITS_PER_BYTE; bit++) {
            crc16 = (crc16 & HDLC_CRC16_CCITT_MSB_MASK) ? (uint16_t)((crc16 << 1) ^ HDLC_CRC16_CCITT_POLY) : (uint16_t)(crc16 << 1);
        }
    }
    return crc16 & HDLC_CRC16_CCITT_INIT;
}

uint16_t hdlc_crc16(const uint8_t* payload, size_t num_bytes)
{
    return crc16_ccitt_false(payload, num_bytes);
}
