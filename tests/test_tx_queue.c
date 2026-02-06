#include <string.h>

#include "drivers/tx_queue.h"
#include "unity.h"

static void init_queue(TX_QUEUE_T* queue, uint8_t* buffer)
{
    memset(queue, 0, sizeof(*queue));
    TEST_ASSERT_EQUAL(E2S_OK, tx_queue_init(queue, buffer));
}

void test_tx_queue_init_empty(void)
{
    TX_QUEUE_T queue = {0};
    uint8_t buffer[TX_FRAME_QUEUE_SIZE * sizeof(TX_QUEUE_ENTRY_T)];

    TEST_ASSERT_EQUAL(E2S_OK, tx_queue_init(&queue, buffer));
    TEST_ASSERT_TRUE(tx_queue_is_empty(&queue));
    TEST_ASSERT_FALSE(queue.queue_touched);
}

void test_tx_queue_enqueue_single(void)
{
    TX_QUEUE_T queue = {0};
    uint8_t buffer[TX_FRAME_QUEUE_SIZE * sizeof(TX_QUEUE_ENTRY_T)];
    init_queue(&queue, buffer);

    uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
    UDP_FRAME_T frame = {
        .payload = payload,
        .length  = sizeof(payload),
    };

    TEST_ASSERT_EQUAL(E2S_OK, tx_queue_enqueue_udp_frame(&queue, &frame));
    TEST_ASSERT_FALSE(tx_queue_is_empty(&queue));
    TEST_ASSERT_TRUE(queue.queue_touched);
}

void test_tx_queue_full_rejected(void)
{
    TX_QUEUE_T queue = {0};
    uint8_t buffer[TX_FRAME_QUEUE_SIZE * sizeof(TX_QUEUE_ENTRY_T)];
    init_queue(&queue, buffer);

    uint8_t payload[] = {0x10, 0x11};
    UDP_FRAME_T frame = {
        .payload = payload,
        .length  = sizeof(payload),
    };

    for (size_t i = 0; i < TX_FRAME_QUEUE_SIZE; i++)
    {
        TEST_ASSERT_EQUAL(E2S_OK, tx_queue_enqueue_udp_frame(&queue, &frame));
    }

    TEST_ASSERT_EQUAL(E2S_ERR_TX_QUEUE_FULL, tx_queue_enqueue_udp_frame(&queue, &frame));
}

void test_tx_queue_oversized_payload_rejected(void)
{
    TX_QUEUE_T queue = {0};
    uint8_t buffer[TX_FRAME_QUEUE_SIZE * sizeof(TX_QUEUE_ENTRY_T)];
    init_queue(&queue, buffer);

    uint8_t payload[1497];
    memset(payload, 0x00, sizeof(payload));
    UDP_FRAME_T frame = {
        .payload = payload,
        .length  = sizeof(payload),
    };

    TEST_ASSERT_EQUAL(E2S_ERR_HDLC_ENCODE_FAILED, tx_queue_enqueue_udp_frame(&queue, &frame));
}
