#include <stdint.h>
#include <string.h>

#include "system/ringbuffer.h"
#include "unity.h"

void test_ringbuffer_init_null_buffer(void)
{
    Ringbuffer rb = {0};
    TEST_ASSERT_EQUAL(-1, RbInit(&rb, NULL, 4, sizeof(uint8_t)));
}

void test_ringbuffer_init_sets_state(void)
{
    Ringbuffer rb = {0};
    uint8_t storage[4];
    memset(storage, 0xAA, sizeof(storage));

    TEST_ASSERT_EQUAL(0, RbInit(&rb, storage, 4, sizeof(uint8_t)));
    TEST_ASSERT_EQUAL(0, rb.count);
    TEST_ASSERT_EQUAL(4, rb.capacity);
    TEST_ASSERT_EQUAL(sizeof(uint8_t), rb.itemSizeInByte);
    TEST_ASSERT_EQUAL_PTR(storage, rb.head);
    TEST_ASSERT_EQUAL_PTR(storage, rb.tail);
}

void test_ringbuffer_push_pop_order_with_wrap(void)
{
    Ringbuffer rb = {0};
    uint8_t storage[4];
    TEST_ASSERT_EQUAL(0, RbInit(&rb, storage, 4, sizeof(uint8_t)));

    for (uint8_t i = 1; i <= 4; i++)
    {
        TEST_ASSERT_EQUAL(0, RbPushBack(&rb, &i));
    }

    uint8_t out = 0;
    TEST_ASSERT_EQUAL(0, RbPopFront(&rb, &out));
    TEST_ASSERT_EQUAL(1, out);
    TEST_ASSERT_EQUAL(0, RbPopFront(&rb, &out));
    TEST_ASSERT_EQUAL(2, out);

    uint8_t v5 = 5;
    uint8_t v6 = 6;
    TEST_ASSERT_EQUAL(0, RbPushBack(&rb, &v5));
    TEST_ASSERT_EQUAL(0, RbPushBack(&rb, &v6));

    TEST_ASSERT_EQUAL(0, RbPopFront(&rb, &out));
    TEST_ASSERT_EQUAL(3, out);
    TEST_ASSERT_EQUAL(0, RbPopFront(&rb, &out));
    TEST_ASSERT_EQUAL(4, out);
    TEST_ASSERT_EQUAL(0, RbPopFront(&rb, &out));
    TEST_ASSERT_EQUAL(5, out);
    TEST_ASSERT_EQUAL(0, RbPopFront(&rb, &out));
    TEST_ASSERT_EQUAL(6, out);

    TEST_ASSERT_EQUAL(-1, RbPopFront(&rb, &out));
}

void test_ringbuffer_push_full_fails(void)
{
    Ringbuffer rb = {0};
    uint8_t storage[2];
    TEST_ASSERT_EQUAL(0, RbInit(&rb, storage, 2, sizeof(uint8_t)));

    uint8_t a = 1;
    uint8_t b = 2;
    uint8_t c = 3;
    TEST_ASSERT_EQUAL(0, RbPushBack(&rb, &a));
    TEST_ASSERT_EQUAL(0, RbPushBack(&rb, &b));
    TEST_ASSERT_EQUAL(-1, RbPushBack(&rb, &c));
    TEST_ASSERT_EQUAL(2, rb.count);
}

void test_ringbuffer_pop_empty_fails(void)
{
    Ringbuffer rb = {0};
    uint8_t storage[2];
    TEST_ASSERT_EQUAL(0, RbInit(&rb, storage, 2, sizeof(uint8_t)));

    uint8_t out = 0;
    TEST_ASSERT_EQUAL(-1, RbPopFront(&rb, &out));
}

void test_ringbuffer_push_wrap_overwrites_oldest(void)
{
    Ringbuffer rb = {0};
    uint8_t storage[3];
    TEST_ASSERT_EQUAL(0, RbInit(&rb, storage, 3, sizeof(uint8_t)));

    uint8_t v1 = 1;
    uint8_t v2 = 2;
    uint8_t v3 = 3;
    uint8_t v4 = 4;

    TEST_ASSERT_EQUAL(0, RbPushBack(&rb, &v1));
    TEST_ASSERT_EQUAL(0, RbPushBack(&rb, &v2));
    TEST_ASSERT_EQUAL(0, RbPushBack(&rb, &v3));

    RbPushBackWrap(&rb, &v4);

    uint8_t out = 0;
    TEST_ASSERT_EQUAL(0, RbPopFront(&rb, &out));
    TEST_ASSERT_EQUAL(2, out);
    TEST_ASSERT_EQUAL(0, RbPopFront(&rb, &out));
    TEST_ASSERT_EQUAL(3, out);
    TEST_ASSERT_EQUAL(0, RbPopFront(&rb, &out));
    TEST_ASSERT_EQUAL(4, out);
    TEST_ASSERT_EQUAL(-1, RbPopFront(&rb, &out));
}
