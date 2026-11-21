#include <stddef.h>
#include <stdint.h>

#include "drivers/w5500_driver.h"
#include "mock_w5500_socket.h"
#include "unity.h"

static UDP_CONFIG_T test_destination(void)
{
    UDP_CONFIG_T destination = {
        .ip_address = {192U, 168U, 29U, 1U},
        .port       = 6969U,
    };
    return destination;
}

void test_w5500_udp_tx_accepts_exact_length_send(void)
{
    uint8_t      payload[]   = {0x01U, 0x02U, 0x03U, 0x04U};
    UDP_FRAME_T  frame       = {.payload = payload, .length = sizeof(payload)};
    UDP_CONFIG_T destination = test_destination();

    mock_w5500_socket_reset();
    mock_w5500_socket_set_send_result((int32_t)sizeof(payload));

    TEST_ASSERT_EQUAL(E2S_OK, w5500_udp_tx(&destination, &frame));
}

void test_w5500_udp_tx_rejects_negative_send(void)
{
    uint8_t      payload[]   = {0x01U, 0x02U, 0x03U, 0x04U};
    UDP_FRAME_T  frame       = {.payload = payload, .length = sizeof(payload)};
    UDP_CONFIG_T destination = test_destination();

    mock_w5500_socket_reset();
    mock_w5500_socket_set_send_result(-1);

    TEST_ASSERT_EQUAL(E2S_ERR_W5500_SEND_FAILED, w5500_udp_tx(&destination, &frame));
}

void test_w5500_udp_tx_rejects_short_send(void)
{
    uint8_t      payload[]   = {0x01U, 0x02U, 0x03U, 0x04U};
    UDP_FRAME_T  frame       = {.payload = payload, .length = sizeof(payload)};
    UDP_CONFIG_T destination = test_destination();

    mock_w5500_socket_reset();
    mock_w5500_socket_set_send_result((int32_t)sizeof(payload) - 1);

    TEST_ASSERT_EQUAL(E2S_ERR_W5500_SEND_FAILED, w5500_udp_tx(&destination, &frame));
}

void test_w5500_udp_tx_rejects_invalid_frame(void)
{
    UDP_CONFIG_T destination = test_destination();
    UDP_FRAME_T  frame       = {.payload = NULL, .length = 1U};

    TEST_ASSERT_EQUAL(E2S_ERR_W5500_SEND_FAILED, w5500_udp_tx(NULL, &frame));
    TEST_ASSERT_EQUAL(E2S_ERR_W5500_SEND_FAILED, w5500_udp_tx(&destination, NULL));
    TEST_ASSERT_EQUAL(E2S_ERR_W5500_SEND_FAILED, w5500_udp_tx(&destination, &frame));
}
