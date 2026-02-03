/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/drivers/w5500_driver.h
 * Purpose: W5500 driver interface.
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef W5500_DRIVER_H
#define W5500_DRIVER_H
// Related headers

// Standard library headers
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

// Project Headers
#include "socket.h"
#include "w5500.h"
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"
#include "wizchip_spi.h"

// Generated headers

// Defaults
#define DEFAULT_MAC_ADDR {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}
#define DEFAULT_IP_ADDR {192, 168, 29, 20}
#define DEFAULT_SUBNET_MASK {255, 255, 255, 0}
#define DEFAULT_GATEWAY_ADDR {192, 168, 29, 1}
#define DEFAULT_DNS_ADDR {8, 8, 8, 8}
#define DEFAULT_UDP_PORT 6969
#define RX_BUF_SIZE 2048
#define TX_BUF_SIZE 2048

typedef struct
{
    wiz_NetInfo net_info;
    uint8_t     broadcast_address[4];
} NETWORK_CONFIG_T;
typedef struct
{
    uint8_t  ip_address[4];
    uint16_t port;
} UDP_CONFIG_T;

typedef struct
{
    uint8_t* payload;
    size_t   length;
} UDP_FRAME_T;

void w5500_debug_status(void);
void w5500_udp_tx(UDP_CONFIG_T* send_config, UDP_FRAME_T* frame);
void w5500_poll_rx(UDP_CONFIG_T* send_config, UDP_FRAME_T* frame);
void w5500_open_udp_socket(UDP_CONFIG_T* send_config);
void w5500_open_ipraw_socket(void);
void w5500_set_network_defaults(NETWORK_CONFIG_T* config);
void w5500_driver_init(void);

#endif /* W5500_DRIVER_H */
