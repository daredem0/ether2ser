

#ifndef W5500_DRIVER_H
#define W5500_DRIVER_H
#include <stdint.h>
#include <inttypes.h>

// Defaults
#define DEFAULT_MAC_ADDR {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}
#define DEFAULT_IP_ADDR {192, 168, 29, 20}
#define DEFAULT_SUBNET_MASK {255, 255, 255, 0}
#define DEFAULT_GATEWAY_ADDR {192, 168, 29, 1}
#define DEFAULT_DNS_ADDR {8, 8, 8, 8}
#define DEFAULT_UDP_PORT 6969

typedef struct {
    uint8_t ip_address[4];
    uint16_t port;
} UDP_CONFIG_T;

void w5500_debug_status(void);
void w5500_poll_rx(UDP_CONFIG_T *config);
void w5500_open_udp_socket(UDP_CONFIG_T *send_config);
void w5500_open_ipraw_socket(void);
void w5500_set_network_defaults(void);
void w5500_driver_init(void);

#endif /* W5500_DRIVER_H */
