

// Related headers

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

// Project Headers

// Generated headers

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
    printf("  NOTE: Waits for PHY link - requires Ethernet cable!\r\n");
    wizchip_initialize();

    printf("Step 5: Verify chip version...\r\n");
    wizchip_check();

    printf("W5500 initialization successful!\r\n");

    // Configure network settings
    wiz_NetInfo net_info = {
        .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
        .ip = {192, 168, 29, 11},                    // IP address
        .sn = {255, 255, 255, 0},                    // Subnet mask
        .gw = {192, 168, 29, 1},                     // Gateway
        .dns = {8, 8, 8, 8},                         // DNS server
        .dhcp = NETINFO_STATIC};

    network_initialize(net_info);
    print_network_information(net_info);

    printf("\r\nW5500 ready for use!\r\n");

    while (true)
    {
        sleep_ms(1000);
    }
}
