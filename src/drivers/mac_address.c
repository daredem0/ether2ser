/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/drivers/mac_address.c
 * Purpose: Deterministic locally administered Ethernet MAC derivation.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#include "mac_address.h"

#include <string.h>

#define ETHER2SER_MAC_PREFIX_0 0x02U
#define ETHER2SER_MAC_PREFIX_1 0xE2U

void mac_address_from_ipv4(const uint8_t ipv4[IPV4_ADDRESS_LENGTH],
                           uint8_t mac[MAC_ADDRESS_LENGTH])
{
    mac[0] = ETHER2SER_MAC_PREFIX_0;
    mac[1] = ETHER2SER_MAC_PREFIX_1;
    memcpy(&mac[2], ipv4, IPV4_ADDRESS_LENGTH);
}
