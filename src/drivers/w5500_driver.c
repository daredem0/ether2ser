// Related headers
#include "w5500_driver.h"


// Standard library headers
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

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

#define RX_BUF_SIZE 2048


void w5500_debug_status(void) {
    uint8_t phy_cfg = getPHYCFGR();  // PHY config register
    printf("PHY CFG: 0x%02X (Link %s)\r\n",
           phy_cfg,
           (phy_cfg & 0x01) ? "UP" : "DOWN");

    uint8_t mode = getSn_MR(IP_SOCKET);  // Socket mode register
    uint8_t status = getSn_SR(IP_SOCKET); // Socket status
    printf("Socket Mode: 0x%02X, Status: 0x%02X\r\n", mode, status);
}

void w5500_poll_rx(UDP_CONFIG_T *send_config){
    uint8_t recv_buf[RX_BUF_SIZE]; // Payload buffer
    int32_t recv_len = recvfrom(UDP_SOCKET, recv_buf, sizeof(recv_buf), send_config->ip_address, &(send_config->port));

    if (recv_len > 0){
        printf("RX %ld bytes from %u.%u.%u.%u:%u\r\n",
            (long)recv_len, send_config->ip_address[0],
            send_config->ip_address[1], send_config->ip_address[2],
            send_config->ip_address[3], send_config->port);

        // Print first 16 bytes as hex (or less if packet is smaller)
        printf("Data: ");
        int print_len = (recv_len < 16) ? recv_len : 16;
        for (int i = 0; i < print_len; i++) {
            printf("%02X ", recv_buf[i]);
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
    int8_t ret = socket(UDP_SOCKET, Sn_MR_UDP, config->port, 0);
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

void w5500_set_network_defaults(void){

    // Configure network settings
    wiz_NetInfo net_info = {
        .mac = DEFAULT_MAC_ADDR,
        .ip = DEFAULT_IP_ADDR,
        .sn = DEFAULT_SUBNET_MASK,
        .gw = DEFAULT_GATEWAY_ADDR,
        .dns = DEFAULT_DNS_ADDR,
        .dhcp = NETINFO_STATIC
    };

    network_initialize(net_info);
    print_network_information(net_info);
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
