/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/drivers/w5500_driver.c
 * Purpose: W5500 driver implementation and helpers.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "w5500_driver.h"

// Standard library headers
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Library Headers
#include "socket.h"
#include "w5500.h"
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"
#include "wizchip_spi.h"

// Project Headers
#include "system/common.h"
#include "system/error.h"

// Generated headers

// Default network configuration

// Socket
#define IP_SOCKET 0 // Socket number for IPRAW
#define UDP_SOCKET 1
#define UDP_MAX_PAYLOAD_BYTES 1472U
#define UDP_W5500_OVERHEAD_BYTES 8U
#define UDP_RX_REQUIRED_BYTES (UDP_MAX_PAYLOAD_BYTES + UDP_W5500_OVERHEAD_BYTES)

void w5500_poll_udp_buffer_full_events(uint64_t* rx_full_enter_events,
                                       uint64_t* tx_full_enter_events)
{
    static uint64_t rx_events   = 0;
    static uint64_t tx_events   = 0;
    static bool     rx_was_full = false;
    static bool     tx_was_full = false;

    uint16_t rx_used_bytes = getSn_RX_RSR(UDP_SOCKET);
    uint16_t rx_cap_bytes  = (uint16_t)getSn_RXBUF_SIZE(UDP_SOCKET) * 1024U;

    uint16_t tx_free_bytes = getSn_TX_FSR(UDP_SOCKET);
    uint16_t tx_cap_bytes  = (uint16_t)getSn_TXBUF_SIZE(UDP_SOCKET) * 1024U;

    // For UDP RX overload visibility, "full" means there is not enough free
    // room for one max-size datagram (+ W5500 UDP metadata), which is when
    // incoming drops can start even before 100% occupancy.
    uint16_t rx_free_bytes =
        (rx_used_bytes < rx_cap_bytes) ? (uint16_t)(rx_cap_bytes - rx_used_bytes) : (uint16_t)0;
    bool rx_is_full = (rx_cap_bytes > 0U) && (rx_free_bytes < UDP_RX_REQUIRED_BYTES);
    bool tx_is_full = (tx_cap_bytes > 0U) && (tx_free_bytes == 0U);

    if (rx_is_full && !rx_was_full)
    {
        rx_events++;
    }
    if (tx_is_full && !tx_was_full)
    {
        tx_events++;
    }

    rx_was_full = rx_is_full;
    tx_was_full = tx_is_full;

    if (rx_full_enter_events)
    {
        *rx_full_enter_events = rx_events;
    }
    if (tx_full_enter_events)
    {
        *tx_full_enter_events = tx_events;
    }
}

static void ipv4_calc_broadcast_u8(const uint8_t ip_addr[4], const uint8_t mask[4],
                                   uint8_t bcast[4])
{
    for (int i = 0; i < 4; i++)
    {
        bcast[i] = (uint8_t)((ip_addr[i] & mask[i]) | (uint8_t)(~mask[i]));
    }
}

void w5500_debug_status(void)
{
    uint8_t phy_cfg = getPHYCFGR(); // PHY config register
    LOG_DEBUG("PHY CFG: 0x%02X (Link %s)\r\n", phy_cfg, (phy_cfg & 0x01) ? "UP" : "DOWN");

    uint8_t mode   = getSn_MR(IP_SOCKET); // Socket mode register
    uint8_t status = getSn_SR(IP_SOCKET); // Socket status
    LOG_DEBUG("Socket Mode: 0x%02X, Status: 0x%02X\r\n", mode, status);
}

void w5500_udp_tx(UDP_CONFIG_T* send_config, const UDP_FRAME_T* frame)
{
    int32_t sent_len = sendto(UDP_SOCKET, frame->payload, (uint16_t)frame->length,
                              send_config->ip_address, send_config->port);
    LOG_DEBUG("TX %ld bytes to %u.%u.%u.%u:%u\r\n", (long)frame->length, send_config->ip_address[0],
              send_config->ip_address[1], send_config->ip_address[2], send_config->ip_address[3],
              send_config->port);
    if (sent_len < 0)
    {
        LOG_DEBUG("sendto() error %ld\r\n", (long)sent_len);
    }
}

bool w5500_poll_rx(UDP_CONFIG_T* send_config, UDP_FRAME_T* frame)
{
    // Clean up frame buffer first to ensure no data corruption
    if (frame->length > 0)
    {
        memset(frame->payload, 0, frame->length);
        frame->length = 0;
    }
    int32_t recv_len = recvfrom(UDP_SOCKET, frame->payload, RX_BUF_SIZE, send_config->ip_address,
                                &(send_config->port));
    if (recv_len > 0)
    {
        frame->length = (size_t)recv_len;
        LOG_DEBUG("RX %ld bytes from %u.%u.%u.%u:%u\r\n", (long)recv_len,
                  send_config->ip_address[0], send_config->ip_address[1],
                  send_config->ip_address[2], send_config->ip_address[3], send_config->port);

        // Print first 16 bytes as hex (or less if packet is smaller)
        LOG_DEBUG("Data: ");
        int print_len = (recv_len < 16) ? recv_len : 16;
        for (int i = 0; i < print_len; i++)
        {
            LOG_DEBUG("%02X ", frame->payload[i]);
        }
        LOG_DEBUG("\r\n");
        return true;
    }
    return false;
}

e2s_error_t w5500_open_ipraw_socket(void)
{
    int8_t ret = socket(IP_SOCKET, Sn_MR_IPRAW, 0, 0);
    if (ret != IP_SOCKET)
    {
        LOG_ERROR("W5500: socket() failed, ret=%d\r\n", (int)ret);
        return E2S_ERR_W5500_SOCKET_OPEN_FAILED;
    }
    LOG_DEBUG("W5500: IPRAW Socket opened successfully in blocking mode\r\n");
    return E2S_OK;
}

e2s_error_t w5500_open_udp_socket(UDP_CONFIG_T* config)
{
    LOG_DEBUG("Starting UDP echo on port %u...\r\n", config->port);

    // socket() allocates one of the W5500's 8 hardware sockets
    // Parameters: socket_number, protocol_mode, local_port, flags
    // Returns: socket_number on success, negative on failure
    int8_t ret = socket(UDP_SOCKET, Sn_MR_UDP, config->port, SF_IO_NONBLOCK);
    if (ret != UDP_SOCKET)
    {
        LOG_ERROR("W5500: socket() failed, ret=%d\r\n", (int)ret);
        return E2S_ERR_W5500_SOCKET_OPEN_FAILED;
    }
    LOG_DEBUG("W5500: Socket opened successfully in non blocking mode\r\n");
    return E2S_OK;
}
e2s_error_t w5500_reconfigure_udp_socket(UDP_CONFIG_T* config)
{
    close(UDP_SOCKET);
    LOG_DEBUG("W5500: Socket closed\r\n");
    return w5500_open_udp_socket(config);
}

void w5500_set_network(NETWORK_CONFIG_T* config)
{
    ipv4_calc_broadcast_u8(config->net_info.ip, config->net_info.sn, config->broadcast_address);
    LOG_DEBUG("Derived broadcast address %u.%u.%u.%u\r\n", config->broadcast_address[0],
              config->broadcast_address[1], config->broadcast_address[2],
              config->broadcast_address[3]);
    network_initialize(config->net_info);
    print_network_information(config->net_info);
}

static e2s_error_t w5500_apply_socket_mem_map(void)
{
    // [0] TX map, [1] RX map, sockets S0..S7, units = KB
    uint8_t memsize[2][8] = {
        {1, 8, 1, 1, 1, 1, 1, 2},
        {1, 8, 1, 1, 1, 1, 1, 2},
    };

    if (ctlwizchip(CW_INIT_WIZCHIP, (void*)memsize) == -1)
    {
        LOG_ERROR("W5500: failed to apply socket memory map\r\n");
        return E2S_ERR_W5500_INIT_FAILED;
    }

    LOG_INFO("W5500 socket1 buffers: RX=%uKB TX=%uKB\r\n", getSn_RXBUF_SIZE(UDP_SOCKET),
             getSn_TXBUF_SIZE(UDP_SOCKET));
    return E2S_OK;
}

void w5500_set_network_defaults(NETWORK_CONFIG_T* config)
{
    // Configure network settings
    config->net_info = (wiz_NetInfo){.mac  = DEFAULT_MAC_ADDR,
                                     .ip   = DEFAULT_IP_ADDR,
                                     .sn   = DEFAULT_SUBNET_MASK,
                                     .gw   = DEFAULT_GATEWAY_ADDR,
                                     .dns  = DEFAULT_DNS_ADDR,
                                     .dhcp = NETINFO_STATIC};

    w5500_set_network(config);
}

void w5500_driver_init(void)
{
    LOG_DEBUG("W5500: Init PIO SPI\r\n");
    wizchip_spi_initialize();

    LOG_DEBUG("W5500: Init critical section\r\n");
    wizchip_cris_initialize();

    LOG_DEBUG("W5500: Reset chip\r\n");
    wizchip_reset();

    LOG_DEBUG("W5500: Initialize\r\n");
    wizchip_initialize();

    LOG_DEBUG("W5500: Apply socket memory map\r\n");
    if (w5500_apply_socket_mem_map() != E2S_OK)
    {
        fatal_panic(E2S_ERR_W5500_INIT_FAILED);
    }

    LOG_DEBUG("W5500: Verify chip\r\n");
    wizchip_check();

    LOG_DEBUG("W5500: Ready\r\n");
}
