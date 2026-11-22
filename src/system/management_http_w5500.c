#include "system/management_http_w5500.h"

#include "socket.h"
#include "w5500.h"

static uint8_t send_pending_mask;

static uint8_t socket_status(uint8_t socket_number)
{
    return getSn_SR(socket_number);
}

static int8_t socket_open(uint8_t socket_number, uint8_t protocol, uint16_t port,
                          uint8_t flags)
{
    send_pending_mask &= (uint8_t)~(1U << socket_number);
    return socket(socket_number, protocol, port, flags);
}

static int8_t socket_listen(uint8_t socket_number)
{
    return listen(socket_number);
}

static int8_t socket_disconnect(uint8_t socket_number)
{
    send_pending_mask &= (uint8_t)~(1U << socket_number);
    return disconnect(socket_number);
}

static int8_t socket_close(uint8_t socket_number)
{
    send_pending_mask &= (uint8_t)~(1U << socket_number);
    return close(socket_number);
}

static uint16_t socket_rx_available(uint8_t socket_number)
{
    return getSn_RX_RSR(socket_number);
}

static uint16_t socket_tx_available(uint8_t socket_number)
{
    return getSn_TX_FSR(socket_number);
}

static int32_t socket_receive(uint8_t socket_number, uint8_t* data, uint16_t length)
{
    if ((data == NULL) || (length == 0U))
    {
        return SOCKERR_DATALEN;
    }
    const uint8_t status = getSn_SR(socket_number);
    if ((status != SOCK_ESTABLISHED) && (status != SOCK_CLOSE_WAIT))
    {
        return SOCKERR_SOCKSTATUS;
    }
    const uint16_t available = getSn_RX_RSR(socket_number);
    if (available == 0U)
    {
        return SOCK_BUSY;
    }
    if (length > available)
    {
        length = available;
    }
    wiz_recv_data(socket_number, data, length);
    setSn_CR(socket_number, Sn_CR_RECV);
    while (getSn_CR(socket_number) != 0U)
    {
    }
    return (int32_t)length;
}

static int32_t socket_send(uint8_t socket_number, const uint8_t* data, uint16_t length)
{
    if ((data == NULL) || (length == 0U))
    {
        return SOCKERR_DATALEN;
    }
    if ((send_pending_mask & (1U << socket_number)) != 0U)
    {
        return SOCK_BUSY;
    }
    const uint8_t status = getSn_SR(socket_number);
    if ((status != SOCK_ESTABLISHED) && (status != SOCK_CLOSE_WAIT))
    {
        return SOCKERR_SOCKSTATUS;
    }
    if (getSn_TX_FSR(socket_number) < length)
    {
        return SOCK_BUSY;
    }
    wiz_send_data(socket_number, (uint8_t*)data, length);
    setSn_CR(socket_number, Sn_CR_SEND);
    while (getSn_CR(socket_number) != 0U)
    {
    }
    send_pending_mask |= (uint8_t)(1U << socket_number);
    return (int32_t)length;
}

static management_http_send_status_t socket_send_status(uint8_t socket_number)
{
    const uint8_t socket_mask = (uint8_t)(1U << socket_number);
    if ((send_pending_mask & socket_mask) == 0U)
    {
        return MANAGEMENT_HTTP_SEND_COMPLETE;
    }
    const uint8_t interrupts = getSn_IR(socket_number);
    if ((interrupts & Sn_IR_TIMEOUT) != 0U)
    {
        setSn_IR(socket_number, Sn_IR_TIMEOUT);
        send_pending_mask &= (uint8_t)~socket_mask;
        return MANAGEMENT_HTTP_SEND_FAILED;
    }
    if ((interrupts & Sn_IR_SENDOK) != 0U)
    {
        setSn_IR(socket_number, Sn_IR_SENDOK);
        send_pending_mask &= (uint8_t)~socket_mask;
        return MANAGEMENT_HTTP_SEND_COMPLETE;
    }
    const uint8_t status = getSn_SR(socket_number);
    if ((status != SOCK_ESTABLISHED) && (status != SOCK_CLOSE_WAIT))
    {
        send_pending_mask &= (uint8_t)~socket_mask;
        return MANAGEMENT_HTTP_SEND_FAILED;
    }
    return MANAGEMENT_HTTP_SEND_PENDING;
}

const management_http_io_t* management_http_w5500_interface(void)
{
    static const management_http_io_t interface = {
        .status = socket_status,
        .open = socket_open,
        .listen = socket_listen,
        .disconnect = socket_disconnect,
        .close = socket_close,
        .rx_available = socket_rx_available,
        .tx_available = socket_tx_available,
        .receive = socket_receive,
        .send = socket_send,
        .send_status = socket_send_status,
    };
    return &interface;
}
