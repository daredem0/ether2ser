/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/drivers/mac_address.h
 * Purpose: Deterministic locally administered Ethernet MAC derivation.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef MAC_ADDRESS_H
#define MAC_ADDRESS_H

#include <stdint.h>

#define MAC_ADDRESS_LENGTH 6U
#define IPV4_ADDRESS_LENGTH 4U

/**
 * @brief Derive a locally administered unicast MAC from a static IPv4 address.
 *
 * The result is 02:E2:<IPv4 bytes>. Boards sharing an Ethernet segment must
 * therefore have distinct IPv4 addresses, which also guarantees distinct MACs.
 *
 * @param ipv4 Source IPv4 address, four bytes in network order.
 * @param mac Destination MAC address, six bytes.
 */
void mac_address_from_ipv4(const uint8_t ipv4[IPV4_ADDRESS_LENGTH],
                           uint8_t mac[MAC_ADDRESS_LENGTH]);

#endif /* MAC_ADDRESS_H */
