#include <string.h>

#include "drivers/tx_queue.h"
#include "mock_tx_queue_platform.h"
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

    TEST_ASSERT_EQUAL(E2S_ERR_TX_QUEUE_PAYLOAD_TOO_LARGE,
                      tx_queue_enqueue_udp_frame(&queue, &frame));
}

void test_tx_queue_restarts_midframe_after_cts_edge(void)
{
    TX_QUEUE_T queue = {0};
    uint8_t buffer[TX_FRAME_QUEUE_SIZE * sizeof(TX_QUEUE_ENTRY_T)];
    uint8_t payload[] = {0x01U, 0x02U, 0x03U, 0x04U};
    UDP_FRAME_T frame = {.payload = payload, .length = sizeof(payload)};
    size_t bytes_drained = 0U;

    mock_tx_queue_platform_reset();
    tx_queue_set_cts_restart_enabled(true);
    init_queue(&queue, buffer);
    TEST_ASSERT_EQUAL(E2S_OK, tx_queue_enqueue_udp_frame(&queue, &frame));
    TEST_ASSERT_EQUAL(E2S_OK, tx_queue_drain(&queue, 1U, &bytes_drained));
    TEST_ASSERT_EQUAL(1U, bytes_drained);
    TEST_ASSERT_EQUAL(1U, queue.current_entry.offset);

    mock_tx_queue_set_cts_sequence(1U);
    TEST_ASSERT_EQUAL(E2S_OK, tx_queue_drain(&queue, 1U, &bytes_drained));
    TEST_ASSERT_EQUAL(0U, bytes_drained);
    TEST_ASSERT_EQUAL(0U, queue.current_entry.offset);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_tx_queue_hard_reset_count());
}

void test_tx_queue_ignores_cts_edge_when_restart_disabled(void)
{
    TX_QUEUE_T queue = {0};
    uint8_t buffer[TX_FRAME_QUEUE_SIZE * sizeof(TX_QUEUE_ENTRY_T)];
    uint8_t payload[] = {0x01U, 0x02U, 0x03U, 0x04U};
    UDP_FRAME_T frame = {.payload = payload, .length = sizeof(payload)};
    size_t bytes_drained = 0U;

    mock_tx_queue_platform_reset();
    tx_queue_set_cts_restart_enabled(false);
    init_queue(&queue, buffer);
    TEST_ASSERT_EQUAL(E2S_OK, tx_queue_enqueue_udp_frame(&queue, &frame));
    TEST_ASSERT_EQUAL(E2S_OK, tx_queue_drain(&queue, 1U, &bytes_drained));

    mock_tx_queue_set_cts_sequence(1U);
    TEST_ASSERT_EQUAL(E2S_OK, tx_queue_drain(&queue, 1U, &bytes_drained));
    TEST_ASSERT_EQUAL(1U, bytes_drained);
    TEST_ASSERT_EQUAL(2U, queue.current_entry.offset);
    TEST_ASSERT_EQUAL_UINT32(0U, mock_tx_queue_hard_reset_count());
    tx_queue_set_cts_restart_enabled(true);
}

void test_tx_queue_protects_queued_tail_until_pio_completion(void)
{
    TX_QUEUE_T  queue = {0};
    uint8_t     buffer[TX_FRAME_QUEUE_SIZE * sizeof(TX_QUEUE_ENTRY_T)];
    uint8_t     payload[]     = {0x01U, 0x02U, 0x03U, 0x04U};
    UDP_FRAME_T frame         = {.payload = payload, .length = sizeof(payload)};
    size_t      bytes_drained = 0U;

    mock_tx_queue_platform_reset();
    tx_queue_set_cts_restart_enabled(true);
    init_queue(&queue, buffer);
    TEST_ASSERT_EQUAL(E2S_OK, tx_queue_enqueue_udp_frame(&queue, &frame));
    TEST_ASSERT_EQUAL(E2S_OK, tx_queue_drain(&queue, SIZE_MAX, &bytes_drained));
    TEST_ASSERT_EQUAL(queue.current_entry.frame.length, queue.current_entry.offset);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_tx_queue_frame_queued_count());
    TEST_ASSERT_FALSE(tx_queue_is_empty(&queue));

    mock_tx_queue_set_cts_sequence(1U);
    TEST_ASSERT_EQUAL(E2S_OK, tx_queue_drain(&queue, 1U, &bytes_drained));
    TEST_ASSERT_EQUAL(0U, queue.current_entry.offset);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_tx_queue_hard_reset_count());

    TEST_ASSERT_EQUAL(E2S_OK, tx_queue_drain(&queue, SIZE_MAX, &bytes_drained));
    TEST_ASSERT_EQUAL_UINT32(2U, mock_tx_queue_frame_queued_count());
    mock_tx_queue_set_cts_sequence(2U);
    mock_tx_queue_set_frame_complete(true);
    mock_tx_queue_set_poll_idle(true);
    TEST_ASSERT_EQUAL(E2S_OK, tx_queue_drain(&queue, 1U, &bytes_drained));
    TEST_ASSERT_TRUE(tx_queue_is_empty(&queue));
    TEST_ASSERT_EQUAL_UINT32(1U, mock_tx_queue_hard_reset_count());
}

void test_tx_queue_waits_for_rts_release_between_queued_frames(void)
{
    TX_QUEUE_T  queue = {0};
    uint8_t     buffer[TX_FRAME_QUEUE_SIZE * sizeof(TX_QUEUE_ENTRY_T)];
    uint8_t     payload[]     = {0x01U, 0x02U, 0x03U, 0x04U};
    UDP_FRAME_T frame         = {.payload = payload, .length = sizeof(payload)};
    size_t      bytes_drained = 0U;

    mock_tx_queue_platform_reset();
    tx_queue_set_cts_restart_enabled(true);
    init_queue(&queue, buffer);
    TEST_ASSERT_EQUAL(E2S_OK, tx_queue_enqueue_udp_frame(&queue, &frame));
    TEST_ASSERT_EQUAL(E2S_OK, tx_queue_enqueue_udp_frame(&queue, &frame));

    TEST_ASSERT_EQUAL(E2S_OK, tx_queue_drain(&queue, SIZE_MAX, &bytes_drained));
    mock_tx_queue_set_frame_complete(true);
    TEST_ASSERT_EQUAL(E2S_OK, tx_queue_drain(&queue, SIZE_MAX, &bytes_drained));
    TEST_ASSERT_EQUAL_UINT32(1U, mock_tx_queue_poll_count());
    TEST_ASSERT_FALSE(tx_queue_is_empty(&queue));

    mock_tx_queue_set_poll_idle(true);
    TEST_ASSERT_EQUAL(E2S_OK, tx_queue_drain(&queue, SIZE_MAX, &bytes_drained));
    TEST_ASSERT_EQUAL_UINT32(2U, mock_tx_queue_poll_count());
    TEST_ASSERT_FALSE(tx_queue_is_empty(&queue));

    TEST_ASSERT_EQUAL(E2S_OK, tx_queue_drain(&queue, SIZE_MAX, &bytes_drained));
    TEST_ASSERT_GREATER_THAN(0U, bytes_drained);
}
