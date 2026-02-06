#include <stdint.h>

#include "system/event_queue.h"
#include "unity.h"

static event_t make_event(event_type_t type, uintptr_t data, size_t len)
{
    event_t ev = {
        .type     = type,
        .data.ptr = (const void*)data,
        .data_len = len,
        .is_inline = false,
    };
    return ev;
}

void test_event_queue_empty_after_init(void)
{
    event_queue_init();
    TEST_ASSERT_TRUE(event_queue_is_empty());
    TEST_ASSERT_FALSE(event_queue_is_full());
}

void test_event_queue_push_pop_single(void)
{
    event_queue_init();
    event_t in = make_event(EV_CLI_LINE, 0x1234, 4);
    TEST_ASSERT_TRUE(event_queue_push(&in));

    event_t out = {0};
    TEST_ASSERT_TRUE(event_queue_pop(&out));
    TEST_ASSERT_EQUAL(in.type, out.type);
    TEST_ASSERT_EQUAL_PTR(in.data.ptr, out.data.ptr);
    TEST_ASSERT_EQUAL(in.data_len, out.data_len);
    TEST_ASSERT_TRUE(event_queue_is_empty());
}

void test_event_queue_full_when_capacity_reached(void)
{
    event_queue_init();
    for (size_t i = 0; i < EVENT_QUEUE_CAPACITY - 1; i++)
    {
        event_t ev = make_event(EV_UDP_RX, (uintptr_t)i, i);
        TEST_ASSERT_TRUE(event_queue_push(&ev));
    }
    TEST_ASSERT_TRUE(event_queue_is_full());
    event_t extra = make_event(EV_UDP_RX, 0xdead, 1);
    TEST_ASSERT_FALSE(event_queue_push(&extra));
}

void test_event_queue_pop_empty_fails(void)
{
    event_queue_init();
    event_t out = {0};
    TEST_ASSERT_FALSE(event_queue_pop(&out));
}

void test_event_queue_wraparound_preserves_order(void)
{
    event_queue_init();

    for (size_t i = 0; i < EVENT_QUEUE_CAPACITY - 1; i++)
    {
        event_t ev = make_event(EV_UDP_RX, (uintptr_t)(0x1000 + i), i);
        TEST_ASSERT_TRUE(event_queue_push(&ev));
    }

    for (size_t i = 0; i < (EVENT_QUEUE_CAPACITY - 1) / 2; i++)
    {
        event_t out = {0};
        TEST_ASSERT_TRUE(event_queue_pop(&out));
        TEST_ASSERT_EQUAL(EV_UDP_RX, out.type);
        TEST_ASSERT_EQUAL_PTR((const void*)(0x1000 + i), out.data.ptr);
        TEST_ASSERT_EQUAL(i, out.data_len);
    }

    for (size_t i = 0; i < (EVENT_QUEUE_CAPACITY - 1) / 2; i++)
    {
        event_t ev = make_event(EV_CLI_LINE, (uintptr_t)(0x2000 + i), i + 10);
        TEST_ASSERT_TRUE(event_queue_push(&ev));
    }

    for (size_t i = (EVENT_QUEUE_CAPACITY - 1) / 2; i < EVENT_QUEUE_CAPACITY - 1; i++)
    {
        event_t out = {0};
        TEST_ASSERT_TRUE(event_queue_pop(&out));
        TEST_ASSERT_EQUAL(EV_UDP_RX, out.type);
        TEST_ASSERT_EQUAL_PTR((const void*)(0x1000 + i), out.data.ptr);
        TEST_ASSERT_EQUAL(i, out.data_len);
    }

    for (size_t i = 0; i < (EVENT_QUEUE_CAPACITY - 1) / 2; i++)
    {
        event_t out = {0};
        TEST_ASSERT_TRUE(event_queue_pop(&out));
        TEST_ASSERT_EQUAL(EV_CLI_LINE, out.type);
        TEST_ASSERT_EQUAL_PTR((const void*)(0x2000 + i), out.data.ptr);
        TEST_ASSERT_EQUAL(i + 10, out.data_len);
    }

    TEST_ASSERT_TRUE(event_queue_is_empty());
}
