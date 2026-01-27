// Related headers
#include "w5500_driver.h"


// Standard library headers
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

// Library Headers
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "pico/time.h"
#include "pico/types.h"
#include "hardware/gpio.h"
#include "wizchip_conf.h"
#include "w5500.h"
#include "socket.h"
#include "wizchip_spi.h"
#include "wizchip_qspi_pio.h"

// Project Headers

// Generated headers.h"

// Default network configuration

// Socket
#define IP_SOCKET 0 // Socket number for IPRAW
#define UDP_SOCKET 1


static void ipv4_calc_broadcast_u8(const uint8_t ip[4],
                            const uint8_t mask[4],
                            uint8_t bcast[4])
{
    for (int i = 0; i < 4; i++) {
        bcast[i] = (uint8_t)((ip[i] & mask[i]) | (uint8_t)(~mask[i]));
    }
}


void w5500_debug_status(void) {
    uint8_t phy_cfg = getPHYCFGR();  // PHY config register
    printf("PHY CFG: 0x%02X (Link %s)\r\n",
           phy_cfg,
           (phy_cfg & 0x01) ? "UP" : "DOWN");

    uint8_t mode = getSn_MR(IP_SOCKET);  // Socket mode register
    uint8_t status = getSn_SR(IP_SOCKET); // Socket status
    printf("Socket Mode: 0x%02X, Status: 0x%02X\r\n", mode, status);
}

void w5500_udp_tx(UDP_CONFIG_T *send_config, UDP_FRAME_T *frame){
    int32_t sent_len = sendto(UDP_SOCKET, frame->payload, (uint16_t)frame->length, send_config->ip_address, send_config->port);
    printf("TX %ld bytes to %u.%u.%u.%u:%u\r\n",
        (long)frame->length, send_config->ip_address[0],
        send_config->ip_address[1], send_config->ip_address[2],
        send_config->ip_address[3], send_config->port);
    if (sent_len < 0)
    {
        printf("sendto() error %ld\r\n", (long)sent_len);
    }
}

void w5500_poll_rx(UDP_CONFIG_T *send_config, UDP_FRAME_T *frame){
    // Clean up frame buffer first to ensure no data corruption
    if(frame->length > 0){
        memset(frame->payload, 0, frame->length);
        frame->length = 0;
    }
    int32_t recv_len = recvfrom(UDP_SOCKET, frame->payload, RX_BUF_SIZE, send_config->ip_address, &(send_config->port));
    if (recv_len > 0){
        frame->length = (size_t)recv_len;
        printf("RX %ld bytes from %u.%u.%u.%u:%u\r\n",
            (long)recv_len, send_config->ip_address[0],
            send_config->ip_address[1], send_config->ip_address[2],
            send_config->ip_address[3], send_config->port);

        // Print first 16 bytes as hex (or less if packet is smaller)
        printf("Data: ");
        int print_len = (recv_len < 16) ? recv_len : 16;
        for (int i = 0; i < print_len; i++) {
            printf("%02X ", frame->payload[i]);
        }
        printf("\r\n");
    }
}

void w5500_open_ipraw_socket(void){
    int8_t ret = socket(IP_SOCKET, Sn_MR_IPRAW, 0, 0);
    if (ret != IP_SOCKET)
    {
        printf("W5500: socket() failed, ret=%d\r\n", (int)ret);
        while (true)
        {
            sleep_ms(1000);
        }
    }
    printf("W5500: IPRAW Socket opened successfully in blocking mode\r\n");
}

void w5500_open_udp_socket(UDP_CONFIG_T *config){
    printf("Starting UDP echo on port %u...\r\n", config->port);

    // socket() allocates one of the W5500's 8 hardware sockets
    // Parameters: socket_number, protocol_mode, local_port, flags
    // Returns: socket_number on success, negative on failure
    int8_t ret = socket(UDP_SOCKET, Sn_MR_UDP, config->port, SF_IO_NONBLOCK);
    if (ret != UDP_SOCKET)
    {
        printf("W5500: socket() failed, ret=%d\r\n", (int)ret);
        while (true)
        {
            sleep_ms(1000);
        }
    }
    printf("W5500: Socket opened successfully in blocking mode\r\n");

}


void w5500_set_network_defaults(NETWORK_CONFIG_T *config) {

    // Configure network settings
    config->net_info = (wiz_NetInfo){
        .mac  = DEFAULT_MAC_ADDR,
        .ip   = DEFAULT_IP_ADDR,
        .sn   = DEFAULT_SUBNET_MASK,
        .gw   = DEFAULT_GATEWAY_ADDR,
        .dns  = DEFAULT_DNS_ADDR,
        .dhcp = NETINFO_STATIC
    };
    ipv4_calc_broadcast_u8(config->net_info.ip, config->net_info.sn, config->broadcast_address);
    printf("Derived broadcast address %u.%u.%u.%u\r\n",
        config->broadcast_address[0], config->broadcast_address[1],
        config->broadcast_address[2], config->broadcast_address[3]);
    network_initialize(config->net_info);
    print_network_information(config->net_info);
}

void w5500_driver_init(void)
{
    printf("W5500: Init PIO SPI\r\n");
    wizchip_spi_initialize();

    printf("W5500: Init critical section\r\n");
    wizchip_cris_initialize();

    printf("W5500: Reset chip\r\n");
    wizchip_reset();

    printf("W5500: Initialize\r\n");
    wizchip_initialize();

    printf("W5500: Verify chip\r\n");
    wizchip_check();

    printf("W5500: Ready\r\n");
}
