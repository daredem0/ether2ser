/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/drivers/w5500_driver.h
 * Purpose: W5500 driver interface.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef W5500_DRIVER_H
#define W5500_DRIVER_H
// Related headers

// Standard library headers
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Library Headers
#include "socket.h"
#include "w5500.h"
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"
#include "wizchip_spi.h"

// Project Headers

// Generated headers

/**
 * @name Default Network Settings
 * @{
 */
#define DEFAULT_MAC_ADDR {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}
#define DEFAULT_IP_ADDR {192, 168, 29, 20}
#define DEFAULT_SUBNET_MASK {255, 255, 255, 0}
#define DEFAULT_GATEWAY_ADDR {192, 168, 29, 1}
#define DEFAULT_DNS_ADDR {8, 8, 8, 8}
#define DEFAULT_UDP_PORT 6969
#define RX_BUF_SIZE 8192
#define TX_BUF_SIZE 8192
/** @} */

/**
 * @brief Network configuration wrapper used by the W5500 driver.
 */
typedef struct
{
    /** Full WIZnet network information block. */
    wiz_NetInfo net_info;
    /** Derived broadcast address for convenience TX. */
    uint8_t     broadcast_address[4];
} NETWORK_CONFIG_T;

/**
 * @brief UDP endpoint configuration.
 */
typedef struct
{
    /** IPv4 address. */
    uint8_t  ip_address[4];
    /** UDP port in host byte order. */
    uint16_t port;
} UDP_CONFIG_T;

/**
 * @brief UDP payload container.
 */
typedef struct
{
    /** Pointer to payload bytes. */
    uint8_t* payload;
    /** Payload length in bytes. */
    size_t   length;
} UDP_FRAME_T;

/**
 * @brief Print W5500 socket/PHY debug status.
 */
void w5500_debug_status(void);

/**
 * @brief Send one UDP frame through W5500.
 * @param send_config Destination endpoint.
 * @param frame Frame payload to send.
 */
void w5500_udp_tx(UDP_CONFIG_T* send_config, const UDP_FRAME_T* frame);

/**
 * @brief Poll W5500 for received UDP data.
 * @param send_config Source endpoint output (updated with sender info).
 * @param frame Destination frame buffer for received payload.
 * @return true if a frame was received, false otherwise.
 */
bool w5500_poll_rx(UDP_CONFIG_T* send_config, UDP_FRAME_T* frame);

/**
 * @brief Open configured UDP socket.
 * @param send_config Local endpoint settings.
 */
void w5500_open_udp_socket(UDP_CONFIG_T* send_config);

/**
 * @brief Reconfigure UDP socket with new endpoint settings.
 * @param config Local endpoint settings.
 */
void w5500_reconfigure_udp_socket(UDP_CONFIG_T* config);

/**
 * @brief Open IPRAW socket mode.
 */
void w5500_open_ipraw_socket(void);

/**
 * @brief Fill network config with compile-time defaults.
 * @param config Destination config object.
 */
void w5500_set_network_defaults(NETWORK_CONFIG_T* config);

/**
 * @brief Apply network configuration to W5500 hardware.
 * @param config Source config object.
 */
void w5500_set_network(NETWORK_CONFIG_T* config);

/**
 * @brief Initialize W5500 driver and low-level interface.
 */
void w5500_driver_init(void);

#endif /* W5500_DRIVER_H */
