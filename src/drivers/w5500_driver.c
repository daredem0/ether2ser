// Related headers
#include "w5500_driver.h"


// Standard library headers
#include <stdio.h>
#include <stdint.h>

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

// Defaults
#define DEFAULT_MAC_ADDR {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}
#define DEFAULT_IP_ADDR {192, 168, 29, 20}
#define DEFAULT_SUBNET_MASK {255, 255, 255, 0}
#define DEFAULT_GATEWAY_ADDR {192, 168, 29, 1}
#define DEFAULT_DNS_ADDR {8, 8, 8, 8}

// Socket
#define IP_SOCKET 0 // Socket number for IPRAW

#define IPRAW_RX_BUF_SIZE 1500

void w5500_poll_rx(void){
    if (getSn_RX_RSR(IP_SOCKET) <= 0){
        return;
    }
    uint8_t recv_buf[IPRAW_RX_BUF_SIZE]; // Payload buffer  (MTU MAX)
    int32_t received = recv(IP_SOCKET, recv_buf, IPRAW_RX_BUF_SIZE);
    if (received > 0){
        printf("W5500: Received %d bytes\r\n", received);
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
