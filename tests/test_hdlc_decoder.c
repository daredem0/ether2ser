#include "unity.h"
#include <stddef.h>
#include <stdint.h>
#include "protocol/hdlc_common.h"
#include "protocol/hdlc_decoder.h"


// bool hdlc_decode(const HDLC_FRAME_T *frame, uint8_t *payload, const size_t out_capacity, size_t *payload_length)

void test_decode_empty_frame_no_escape(void) {
    uint8_t frame_buffer[4] = {HDLC_FLAG_BYTE, 0xFF, 0xFF, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 4,
        .capacity = 4
    };
    const size_t out_capacity = 1;
    uint8_t payload_out[out_capacity];
    size_t payload_length = 0;

    bool result = hdlc_decode(&frame, payload_out, out_capacity, &payload_length);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(0, payload_length);
}


void test_decode_one_byte_frame_no_escape(void) {
    uint8_t frame_buffer[4] = {HDLC_FLAG_BYTE, 0xFF, 0xFF, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 4,
        .capacity = 4
    };
    const size_t out_capacity = 1;
    uint8_t payload_out[out_capacity];
    size_t payload_length = 0;

    bool result = hdlc_decode(&frame, payload_out, out_capacity, &payload_length);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(0, payload_length);
}
