#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "protocol/hdlc_common.h"
#include "protocol/hdlc_decoder.h"
#include "protocol/hdlc_encoder.h"
#include "unity.h"

static void encode_frame_or_fail(const uint8_t* payload, size_t payload_len, bool lsb_first,
                                 uint8_t* frame_buffer, size_t frame_capacity, HDLC_FRAME_T* frame)
{
    frame->payload  = frame_buffer;
    frame->length   = 0;
    frame->capacity = frame_capacity;
    TEST_ASSERT_TRUE(hdlc_encode(payload, payload_len, frame, lsb_first));
}

static void decode_expect_ok(const HDLC_FRAME_T* frame, bool lsb_first, const uint8_t* expected,
                             size_t expected_len)
{
    uint8_t out[512];
    size_t  out_len = 0;

    TEST_ASSERT_TRUE(hdlc_decode(frame, out, expected_len + 2, &out_len, lsb_first));
    TEST_ASSERT_EQUAL(expected_len, out_len);
    if (expected_len > 0)
    {
        TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, expected_len);
    }
}

void test_bitstuff_decode_roundtrip_empty_lsb(void)
{
    uint8_t      frame_buffer[64];
    HDLC_FRAME_T frame;
    encode_frame_or_fail(NULL, 0, true, frame_buffer, sizeof(frame_buffer), &frame);
    decode_expect_ok(&frame, true, NULL, 0);
}

void test_bitstuff_decode_roundtrip_one_byte_lsb(void)
{
    uint8_t      payload[] = {0x42};
    uint8_t      frame_buffer[64];
    HDLC_FRAME_T frame;
    encode_frame_or_fail(payload, sizeof(payload), true, frame_buffer, sizeof(frame_buffer), &frame);
    decode_expect_ok(&frame, true, payload, sizeof(payload));
}

void test_bitstuff_decode_roundtrip_multi_byte_lsb(void)
{
    uint8_t      payload[] = {0x01, 0x02, 0x03, 0xA5, 0x7E, 0x7D};
    uint8_t      frame_buffer[128];
    HDLC_FRAME_T frame;
    encode_frame_or_fail(payload, sizeof(payload), true, frame_buffer, sizeof(frame_buffer), &frame);
    decode_expect_ok(&frame, true, payload, sizeof(payload));
}

void test_bitstuff_decode_roundtrip_multi_byte_msb(void)
{
    uint8_t      payload[] = {0x80, 0x13, 0x57, 0x9B, 0xCD, 0xEF};
    uint8_t      frame_buffer[128];
    HDLC_FRAME_T frame;
    encode_frame_or_fail(payload, sizeof(payload), false, frame_buffer, sizeof(frame_buffer), &frame);
    decode_expect_ok(&frame, false, payload, sizeof(payload));
}

void test_bitstuff_decode_roundtrip_large_payload(void)
{
    uint8_t payload[64];
    for (size_t i = 0; i < sizeof(payload); i++)
    {
        payload[i] = (uint8_t)(i * 3u + 1u);
    }
    uint8_t      frame_buffer[512];
    HDLC_FRAME_T frame;
    encode_frame_or_fail(payload, sizeof(payload), true, frame_buffer, sizeof(frame_buffer), &frame);
    decode_expect_ok(&frame, true, payload, sizeof(payload));
}

void test_bitstuff_decode_abort_null_frame(void)
{
    uint8_t out[16];
    size_t  out_len = 123;
    TEST_ASSERT_FALSE(hdlc_decode(NULL, out, sizeof(out), &out_len, true));
    TEST_ASSERT_EQUAL(0, out_len);
}

void test_bitstuff_decode_abort_null_payload(void)
{
    uint8_t      frame_buffer[] = {HDLC_FLAG_BYTE, 0x00, 0x00, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame          = {
                 .payload = frame_buffer, .length = sizeof(frame_buffer), .capacity = sizeof(frame_buffer)};
    size_t out_len = 123;
    TEST_ASSERT_FALSE(hdlc_decode(&frame, NULL, 8, &out_len, true));
    TEST_ASSERT_EQUAL(0, out_len);
}

void test_bitstuff_decode_abort_null_payload_length(void)
{
    uint8_t      frame_buffer[] = {HDLC_FLAG_BYTE, 0x00, 0x00, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame          = {
                 .payload = frame_buffer, .length = sizeof(frame_buffer), .capacity = sizeof(frame_buffer)};
    uint8_t out[16];
    TEST_ASSERT_FALSE(hdlc_decode(&frame, out, sizeof(out), NULL, true));
}

void test_bitstuff_decode_abort_zero_out_capacity(void)
{
    uint8_t      frame_buffer[] = {HDLC_FLAG_BYTE, 0x00, 0x00, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame          = {
                 .payload = frame_buffer, .length = sizeof(frame_buffer), .capacity = sizeof(frame_buffer)};
    uint8_t out[1];
    size_t  out_len = 123;
    TEST_ASSERT_FALSE(hdlc_decode(&frame, out, 0, &out_len, true));
    TEST_ASSERT_EQUAL(0, out_len);
}

void test_bitstuff_decode_abort_missing_start_flag(void)
{
    uint8_t      frame_buffer[] = {0x00, 0xAA, 0xBB, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame          = {
                 .payload = frame_buffer, .length = sizeof(frame_buffer), .capacity = sizeof(frame_buffer)};
    uint8_t out[32];
    size_t  out_len = 123;
    TEST_ASSERT_FALSE(hdlc_decode(&frame, out, sizeof(out), &out_len, true));
    TEST_ASSERT_EQUAL(0, out_len);
}

void test_bitstuff_decode_abort_missing_end_flag(void)
{
    uint8_t      frame_buffer[] = {HDLC_FLAG_BYTE, 0xAA, 0xBB, 0x00};
    HDLC_FRAME_T frame          = {
                 .payload = frame_buffer, .length = sizeof(frame_buffer), .capacity = sizeof(frame_buffer)};
    uint8_t out[32];
    size_t  out_len = 123;
    TEST_ASSERT_FALSE(hdlc_decode(&frame, out, sizeof(out), &out_len, true));
    TEST_ASSERT_EQUAL(0, out_len);
}

void test_bitstuff_decode_abort_payload_too_long(void)
{
    uint8_t      payload[] = {0x10, 0x20, 0x30, 0x40, 0x50};
    uint8_t      frame_buffer[128];
    HDLC_FRAME_T frame;
    uint8_t      out[6];
    size_t       out_len = 0;

    encode_frame_or_fail(payload, sizeof(payload), true, frame_buffer, sizeof(frame_buffer), &frame);
    TEST_ASSERT_FALSE(hdlc_decode(&frame, out, sizeof(payload) + 1, &out_len, true));
    TEST_ASSERT_EQUAL(0, out_len);
}

void test_bitstuff_decode_exact_capacity_succeeds(void)
{
    uint8_t      payload[] = {0x12, 0x34, 0x56, 0x78};
    uint8_t      frame_buffer[128];
    HDLC_FRAME_T frame;
    uint8_t      out[16];
    size_t       out_len = 0;

    encode_frame_or_fail(payload, sizeof(payload), true, frame_buffer, sizeof(frame_buffer), &frame);
    TEST_ASSERT_TRUE(hdlc_decode(&frame, out, sizeof(payload) + 2, &out_len, true));
    TEST_ASSERT_EQUAL(sizeof(payload), out_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, out, sizeof(payload));
}

void test_bitstuff_decode_abort_crc_mismatch(void)
{
    uint8_t      payload[] = {0xAB, 0xCD, 0xEF};
    uint8_t      frame_buffer[128];
    HDLC_FRAME_T frame;
    uint8_t      out[32];
    size_t       out_len = 123;

    encode_frame_or_fail(payload, sizeof(payload), true, frame_buffer, sizeof(frame_buffer), &frame);
    frame.payload[1] ^= 0x01;

    TEST_ASSERT_FALSE(hdlc_decode(&frame, out, sizeof(out), &out_len, true));
    TEST_ASSERT_EQUAL(0, out_len);
}

void test_bitstuff_decode_abort_unstuff_violation(void)
{
    uint8_t      frame_buffer[] = {HDLC_FLAG_BYTE, 0xFF, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame          = {
                 .payload = frame_buffer, .length = sizeof(frame_buffer), .capacity = sizeof(frame_buffer)};
    uint8_t out[32];
    size_t  out_len = 123;

    TEST_ASSERT_FALSE(hdlc_decode(&frame, out, sizeof(out), &out_len, true));
    TEST_ASSERT_EQUAL(0, out_len);
}

void test_bitstuff_decode_abort_bit_order_mismatch(void)
{
    uint8_t      payload[] = {0x23, 0x45, 0x67, 0x89};
    uint8_t      frame_buffer[128];
    HDLC_FRAME_T frame;
    uint8_t      out[32];
    size_t       out_len = 0;

    encode_frame_or_fail(payload, sizeof(payload), true, frame_buffer, sizeof(frame_buffer), &frame);
    TEST_ASSERT_TRUE(hdlc_decode(&frame, out, sizeof(out), &out_len, false));
    TEST_ASSERT_EQUAL(sizeof(payload), out_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, out, sizeof(payload));
}
