#include "drivers/w5500_driver.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "socket.h"

#include "drivers/w5500_internal.h"
#include "system/common.h"
#include "system/error.h"

e2s_error_t w5500_udp_tx(UDP_CONFIG_T* send_config, const UDP_FRAME_T* frame)
{
    if (send_config == NULL || frame == NULL || frame->payload == NULL || frame->length == 0U ||
        frame->length > UDP_MAX_PAYLOAD_BYTES)
    {
        return E2S_ERR_W5500_SEND_FAILED;
    }

    int32_t sent_len =
        sendto_W5x00(W5500_UDP_SOCKET, frame->payload, (uint16_t)frame->length,
                     send_config->ip_address, send_config->port);
    if (sent_len != (int32_t)frame->length)
    {
        LOG_DEBUG("sendto() result %ld for %zu-byte datagram\r\n", (long)sent_len,
                  frame->length);
        return E2S_ERR_W5500_SEND_FAILED;
    }

    LOG_DEBUG("TX %zu bytes to %u.%u.%u.%u:%u\r\n", frame->length,
              send_config->ip_address[0], send_config->ip_address[1],
              send_config->ip_address[2], send_config->ip_address[3], send_config->port);
    return E2S_OK;
}

bool w5500_poll_rx(UDP_CONFIG_T* send_config, UDP_FRAME_T* frame)
{
    if (send_config == NULL || frame == NULL || frame->payload == NULL)
    {
        return false;
    }

    if (frame->length > 0U)
    {
        memset(frame->payload, 0, frame->length);
        frame->length = 0U;
    }
    int32_t recv_len =
        recvfrom_W5x00(W5500_UDP_SOCKET, frame->payload, RX_BUF_SIZE, send_config->ip_address,
                       &send_config->port);
    if (recv_len <= 0)
    {
        return false;
    }

    frame->length = (size_t)recv_len;
    LOG_DEBUG("RX %ld bytes from %u.%u.%u.%u:%u\r\n", (long)recv_len,
              send_config->ip_address[0], send_config->ip_address[1],
              send_config->ip_address[2], send_config->ip_address[3], send_config->port);

    LOG_DEBUG("Data: ");
    int32_t print_len = recv_len < 16 ? recv_len : 16;
    for (int32_t i = 0; i < print_len; i++)
    {
        LOG_DEBUG("%02X ", frame->payload[i]);
    }
    LOG_DEBUG("\r\n");
    return true;
}
