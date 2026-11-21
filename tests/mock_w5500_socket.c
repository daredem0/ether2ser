#include <stdint.h>

#include "mock_w5500_socket.h"

static int32_t send_result;

void mock_w5500_socket_reset(void)
{
    send_result = 0;
}

void mock_w5500_socket_set_send_result(int32_t result)
{
    send_result = result;
}

int32_t sendto_W5x00(uint8_t socket, uint8_t* buffer, uint16_t length, uint8_t* address,
                     uint16_t port)
{
    (void)socket;
    (void)buffer;
    (void)length;
    (void)address;
    (void)port;
    return send_result;
}

int32_t recvfrom_W5x00(uint8_t socket, uint8_t* buffer, uint16_t length, uint8_t* address,
                       uint16_t* port)
{
    (void)socket;
    (void)buffer;
    (void)length;
    (void)address;
    (void)port;
    return 0;
}
