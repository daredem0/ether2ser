/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/examples/w55_udp_echo/main.c
 * Purpose: W5500 UDP echo example.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers

// Standard library headers
#include <stdint.h>
#include <stdio.h>

// Project Headers
#include "hardware/gpio.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/types.h"
#include "socket.h"
#include "w5500.h"
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"
#include "wizchip_spi.h"

// Generated headers

// Access to the WIZnet port layer's SPI handle (defined in wizchip_spi.c)
extern wiznet_spi_handle_t spi_handle;

#define UDP_SOCK 0
#define UDP_PORT 6969 // change to 5000 if you prefer

static void w5500_init_no_linkwait(void)
{
    // Register PIO-SPI callbacks from the port layer (spi_handle comes from wizchip_spi.h)
    (*spi_handle)->frame_end();
    reg_wizchip_spi_cbfunc((*spi_handle)->read_byte, (*spi_handle)->write_byte);
    reg_wizchip_spiburst_cbfunc((*spi_handle)->read_buffer, (*spi_handle)->write_buffer);
    reg_wizchip_cs_cbfunc((*spi_handle)->frame_start, (*spi_handle)->frame_end);

    // 2 KB per socket (8 sockets) for TX and RX
    uint8_t memsize[2][8] = {
        {2, 2, 2, 2, 2, 2, 2, 2}, // TX buffers
        {2, 2, 2, 2, 2, 2, 2, 2}  // RX buffers
    };

    if (ctlwizchip(CW_INIT_WIZCHIP, (void *)memsize) == -1)
    {
        printf("W5500 init failed (CW_INIT_WIZCHIP)\r\n");
    }
}

static void configure_network(void)
{
    // Configure network settings
    wiz_NetInfo net_info = {
        .mac = {0x02, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
        .ip = {192, 168, 29, 11},                    // IP address
        .sn = {255, 255, 255, 0},                    // Subnet mask
        .gw = {192, 168, 29, 1},                     // Gateway
        .dns = {8, 8, 8, 8},                         // DNS server
        .dhcp = NETINFO_STATIC};

    network_initialize(net_info);
    print_network_information(net_info);
}

static void run_udp_echo_server(void)
{
    // Create a UDP socket bound to our chosen port. This socket will listen for
    // incoming UDP packets and can send responses back to the source address/port.
    printf("Starting UDP echo on port %u...\r\n", UDP_PORT);

    // socket() allocates one of the W5500's 8 hardware sockets
    // Parameters: socket_number, protocol_mode, local_port, flags
    // Returns: socket_number on success, negative on failure
    int8_t ret = socket(UDP_SOCK, Sn_MR_UDP, UDP_PORT, 0);
    if (ret != UDP_SOCK)
    {
        printf("socket() failed, ret=%d\r\n", (int)ret);
        while (true)
        {
            sleep_ms(1000);
        }
    }
    printf("Socket opened successfully in blocking mode\r\n");

    // Allocate buffers for UDP packet data and sender information
    uint8_t recv_buf[2048]; // Payload buffer (max UDP payload in W5500)
    uint8_t remote_ip[4];   // Remote (sender) IP address
    uint16_t remote_port;   // Remote (sender) port number

    printf("Entering main echo loop (blocking mode)...\r\n");

    // Main echo loop: receive packets and send them back to the sender
    while (true)
    {
        // Blocking receive: waits for a UDP packet to arrive
        // Returns: number of bytes received (>0), <0 on error
        // Also fills remote_ip[] and remote_port with sender's address/port
        int32_t recv_len = recvfrom(UDP_SOCK, recv_buf, sizeof(recv_buf), remote_ip, &remote_port);

        if (recv_len > 0)
        {
            // Packet received - log the sender and byte count
            printf("RX %ld bytes from %u.%u.%u.%u:%u\r\n",
                   (long)recv_len, remote_ip[0], remote_ip[1], remote_ip[2], remote_ip[3], remote_port);

            // Echo the packet back to the sender
            // Parameters: socket_number, data_buffer, data_length, dest_ip, dest_port
            int32_t sent_len = sendto(UDP_SOCK, recv_buf, (uint16_t)recv_len, remote_ip, remote_port);
            if (sent_len < 0)
            {
                printf("sendto() error %ld\r\n", (long)sent_len);
            }
        }
        else
        {
            // Error occurred during receive
            printf("recvfrom() error %ld\r\n", (long)recv_len);
        }
    }
}

int main(void)
{
    stdio_init_all();
    sleep_ms(2000);

    printf("==========================================================\r\n");
    printf("W55RP20-EVB-PICO W5500 Initialization Test\r\n");
    printf("==========================================================\r\n");

    // Initialize W5500 with more verbose debugging
    printf("Step 1: Init PIO SPI...\r\n");
    wizchip_spi_initialize();

    printf("Step 2: Init critical section...\r\n");
    wizchip_cris_initialize();

    printf("Step 3: Reset W5500...\r\n");
    wizchip_reset();

    printf("Step 4: Call wizchip_initialize...\r\n");
    // w5500_init_no_linkwait(); // use this for skip
    wizchip_initialize();

    printf("Step 5: Verify chip version...\r\n");
    wizchip_check();

    printf("W5500 initialization successful!\r\n");

    configure_network();

    run_udp_echo_server();
}
