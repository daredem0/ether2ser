#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "protocol/hdlc_common.h"
#include "protocol/hdlc_encoder.h"
#include "unity.h"

static size_t extract_frame_bits(const HDLC_FRAME_T* frame, bool lsb_first, uint8_t* bits,
                                 size_t max_bits)
{
    size_t bit_count = 0;
    for (size_t i = 1; i + 1 < frame->length; i++)
    {
        uint8_t byte = frame->payload[i];
        for (uint8_t b = 0; b < 8; b++)
        {
            uint8_t bit_pos = lsb_first ? b : (7 - b);
            if (bit_count >= max_bits)
            {
                return bit_count;
            }
            bits[bit_count++] = (byte >> bit_pos) & 0x01;
        }
    }
    return bit_count;
}

static size_t destuff_bits(const uint8_t* in_bits, size_t in_count, uint8_t* out_bits,
                           size_t max_out_bits, bool* stuffed_ok)
{
    size_t  out_count      = 0;
    uint8_t ones_run       = 0;
    bool    skip_next_zero = false;
    *stuffed_ok            = true;

    for (size_t i = 0; i < in_count; i++)
    {
        uint8_t bit = in_bits[i] ? 1 : 0;
        if (skip_next_zero)
        {
            if (bit != 0)
            {
                *stuffed_ok = false;
            }
            skip_next_zero = false;
            ones_run       = 0;
            continue;
        }

        if (out_count < max_out_bits)
        {
            out_bits[out_count++] = bit;
        }

        if (bit)
        {
            ones_run++;
            if (ones_run == 5)
            {
                skip_next_zero = true;
            }
        }
        else
        {
            ones_run = 0;
        }
    }

    if (skip_next_zero)
    {
        *stuffed_ok = false;
    }

    return out_count;
}

static size_t bytes_to_bits(const uint8_t* bytes, size_t byte_count, bool lsb_first,
                            uint8_t* out_bits, size_t max_bits)
{
    size_t bit_count = 0;
    for (size_t i = 0; i < byte_count; i++)
    {
        uint8_t byte = bytes[i];
        for (uint8_t b = 0; b < 8; b++)
        {
            uint8_t bit_pos = lsb_first ? b : (7 - b);
            if (bit_count >= max_bits)
            {
                return bit_count;
            }
            out_bits[bit_count++] = (byte >> bit_pos) & 0x01;
        }
    }
    return bit_count;
}

static size_t count_expected_stuffs(const uint8_t* bits, size_t bit_count)
{
    size_t  stuffed  = 0;
    uint8_t ones_run = 0;
    for (size_t i = 0; i < bit_count; i++)
    {
        if (bits[i])
        {
            ones_run++;
            if (ones_run == 5)
            {
                stuffed++;
                ones_run = 0;
            }
        }
        else
        {
            ones_run = 0;
        }
    }
    return stuffed;
}

static void assert_bitstuff_roundtrip(const uint8_t* payload, size_t payload_len, bool lsb_order)
{
    uint8_t      frame_buffer[256];
    HDLC_FRAME_T frame = {
        .payload  = frame_buffer,
        .length   = 0,
        .capacity = sizeof(frame_buffer),
    };

    bool result = hdlc_encode(payload, payload_len, &frame, lsb_order);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(frame.length > 2);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[frame.length - 1]);

    uint8_t raw_bits[2048];
    uint8_t destuffed_bits[2048];
    size_t  raw_count = extract_frame_bits(&frame, lsb_order, raw_bits, sizeof(raw_bits));

    bool   stuffed_ok = false;
    size_t destuffed_count =
        destuff_bits(raw_bits, raw_count, destuffed_bits, sizeof(destuffed_bits), &stuffed_ok);
    TEST_ASSERT_TRUE(stuffed_ok);

    uint16_t crc = hdlc_crc16(payload, payload_len);
    uint8_t  expected_bytes[256];
    memcpy(expected_bytes, payload, payload_len);
    expected_bytes[payload_len]     = (uint8_t)((crc >> 8) & 0xFF);
    expected_bytes[payload_len + 1] = (uint8_t)(crc & 0xFF);
    size_t expected_byte_count      = payload_len + 2;

    uint8_t expected_bits[2048];
    size_t  expected_bit_count = bytes_to_bits(expected_bytes, expected_byte_count, lsb_order,
                                               expected_bits, sizeof(expected_bits));

    TEST_ASSERT_TRUE(destuffed_count >= expected_bit_count);

    for (size_t i = 0; i < expected_bit_count; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(expected_bits[i], destuffed_bits[i]);
    }

    for (size_t i = expected_bit_count; i < destuffed_count; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(0, destuffed_bits[i]);
    }
}

static void assert_bitstuff_encode_fails(const uint8_t* payload, size_t payload_len,
                                         HDLC_FRAME_T* frame)
{
    bool result = hdlc_encode(payload, payload_len, frame, true);
    TEST_ASSERT_FALSE(result);
    if (frame)
    {
        TEST_ASSERT_EQUAL_UINT8(0, frame->length);
    }
}

void test_bitstuff_encoder_roundtrip_lsb_small(void)
{
    uint8_t payload[] = {0x01, 0x02, 0x03};
    assert_bitstuff_roundtrip(payload, sizeof(payload), true);
}

void test_bitstuff_encoder_roundtrip_lsb_stuffing(void)
{
    uint8_t payload[] = {0xFF, 0xFF, 0x0F};
    assert_bitstuff_roundtrip(payload, sizeof(payload), true);
}

void test_bitstuff_encoder_roundtrip_msb(void)
{
    uint8_t payload[] = {0x80, 0x7E, 0x55, 0xA3};
    assert_bitstuff_roundtrip(payload, sizeof(payload), false);
}

void test_bitstuff_encoder_empty_payload(void)
{
    uint8_t payload[] = {0xAA};
    assert_bitstuff_roundtrip(payload, 0, true);
}

void test_bitstuff_encoder_one_byte_payload(void)
{
    uint8_t payload[] = {0x42};
    assert_bitstuff_roundtrip(payload, sizeof(payload), true);
}

void test_bitstuff_encoder_payload_with_flag_and_escape_bytes(void)
{
    uint8_t payload[] = {0x11, HDLC_FLAG_BYTE, 0x22, HDLC_ESCAPE_BYTE, 0x33};
    assert_bitstuff_roundtrip(payload, sizeof(payload), true);
}

void test_bitstuff_encoder_stuffing_count_lsb(void)
{
    uint8_t      payload[] = {0xFF, 0xFF, 0xFF};
    uint8_t      frame_buffer[256];
    HDLC_FRAME_T frame = {
        .payload  = frame_buffer,
        .length   = 0,
        .capacity = sizeof(frame_buffer),
    };

    bool result = hdlc_encode(payload, sizeof(payload), &frame, true);
    TEST_ASSERT_TRUE(result);

    uint16_t crc = hdlc_crc16(payload, sizeof(payload));
    uint8_t  expected_bytes[256];
    memcpy(expected_bytes, payload, sizeof(payload));
    expected_bytes[sizeof(payload)]     = (uint8_t)((crc >> 8) & 0xFF);
    expected_bytes[sizeof(payload) + 1] = (uint8_t)(crc & 0xFF);
    size_t expected_byte_count          = sizeof(payload) + 2;

    uint8_t expected_bits[2048];
    size_t  expected_bit_count = bytes_to_bits(expected_bytes, expected_byte_count, true,
                                               expected_bits, sizeof(expected_bits));
    size_t  expected_stuffs    = count_expected_stuffs(expected_bits, expected_bit_count);
    TEST_ASSERT_TRUE(expected_stuffs > 0);

    uint8_t raw_bits[2048];
    size_t  raw_count = extract_frame_bits(&frame, true, raw_bits, sizeof(raw_bits));
    TEST_ASSERT_TRUE(raw_count >= expected_bit_count + expected_stuffs);
}

void test_bitstuff_encoder_abort_payload_null_with_length(void)
{
    uint8_t      frame_buffer[16];
    HDLC_FRAME_T frame = {
        .payload  = frame_buffer,
        .length   = 0,
        .capacity = sizeof(frame_buffer),
    };
    assert_bitstuff_encode_fails(NULL, 1, &frame);
}

void test_bitstuff_encoder_abort_frame_null(void)
{
    uint8_t payload[] = {0x01};
    assert_bitstuff_encode_fails(payload, sizeof(payload), NULL);
}

void test_bitstuff_encoder_abort_frame_capacity_invalid(void)
{
    uint8_t      payload[] = {0x01, 0x02, 0x03};
    uint8_t      frame_buffer[1];
    HDLC_FRAME_T frame = {
        .payload  = frame_buffer,
        .length   = 0,
        .capacity = sizeof(frame_buffer),
    };
    assert_bitstuff_encode_fails(payload, sizeof(payload), &frame);
}

void test_bitstuff_encoder_abort_frame_length_nonzero(void)
{
    uint8_t      payload[] = {0x01};
    uint8_t      frame_buffer[16];
    HDLC_FRAME_T frame = {
        .payload  = frame_buffer,
        .length   = 1,
        .capacity = sizeof(frame_buffer),
    };
    assert_bitstuff_encode_fails(payload, sizeof(payload), &frame);
}
