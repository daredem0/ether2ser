#include "unity.h"
#include <stddef.h>
#include <stdint.h>
#include "protocol/hdlc_common.h"
#include "protocol/hdlc_decoder.h"
#include "protocol/hdlc_encoder.h"


// bool hdlc_decode(const HDLC_FRAME_T *frame, uint8_t *payload, const size_t out_capacity, size_t *payload_length)

void test_decode_empty_frame_no_escape(void) {
    // Frame: 7E FF FF 7E
    uint8_t frame_buffer[] = {HDLC_FLAG_BYTE, 0xFF, 0xFF, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = sizeof(frame_buffer),
        .capacity = sizeof(frame_buffer)
    };
    const size_t out_capacity = 10;
    uint8_t payload_out[out_capacity];
    size_t payload_length = 0;

    bool result = hdlc_decode(&frame, payload_out, out_capacity, &payload_length);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(0, payload_length);
}


void test_decode_one_byte_frame_no_escape(void) {
    // Frame: 7E 42 89 76 7E
    uint8_t frame_buffer[] = {HDLC_FLAG_BYTE, 0x42, 0x89, 0x76, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = sizeof(frame_buffer),
        .capacity = sizeof(frame_buffer)
    };
    const size_t out_capacity = 10;
    uint8_t payload_out[out_capacity];
    size_t payload_length = 0;

    bool result = hdlc_decode(&frame, payload_out, out_capacity, &payload_length);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, payload_length);
    TEST_ASSERT_EQUAL_HEX8(0x42, payload_out[0]);
}

void test_decode_three_bytes_frame_no_escape(void){
    // Frame: 7E 01 02 03 AD AD 7E
    uint8_t frame_buffer[] = {HDLC_FLAG_BYTE, 0x01, 0x02, 0x03, 0xAD, 0xAD, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = sizeof(frame_buffer),
        .capacity = sizeof(frame_buffer)
    };
    const size_t out_capacity = 3+2;
    uint8_t payload_out[out_capacity];
    size_t payload_length = 0;

    bool result = hdlc_decode(&frame, payload_out, out_capacity, &payload_length);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(3, payload_length);
    TEST_ASSERT_EQUAL_HEX8(0x01, payload_out[0]);
    TEST_ASSERT_EQUAL_HEX8(0x02, payload_out[1]);
    TEST_ASSERT_EQUAL_HEX8(0x03, payload_out[2]);
}


void test_decode_abort_empty_frame(void){
    // Frame: <none> (encode fails)
    const size_t out_capacity = 3;
    uint8_t payload_out[out_capacity];
    size_t payload_length = 0;
    bool result = hdlc_decode(NULL, payload_out, out_capacity, &payload_length);
    TEST_ASSERT_FALSE(result);
}


void test_decode_abort_empty_frame_2(void){
    // Frame: <none> (encode fails)
    uint8_t frame_buffer[] = {0xFF};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = sizeof(frame_buffer),
        .capacity = sizeof(frame_buffer)
    };
    const size_t out_capacity = 3;
    uint8_t payload_out[out_capacity];
    bool result = hdlc_decode(&frame, payload_out, out_capacity, NULL);
    TEST_ASSERT_FALSE(result);
}

void test_decode_abort_payload_null(void){
    // Frame: <none> (encode fails)
    uint8_t frame_buffer[] = {0xFF};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = sizeof(frame_buffer),
        .capacity = sizeof(frame_buffer)
    };
    const size_t out_capacity = 3;
    uint8_t payload_out[out_capacity];
    size_t payload_length = 0;
    bool result = hdlc_decode(&frame, NULL, out_capacity, &payload_length);
    TEST_ASSERT_FALSE(result);
}

void test_decode_abort_no_capacity(void){
    // Frame: <none> (encode fails)
    uint8_t frame_buffer[] = {0xFF};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = sizeof(frame_buffer),
        .capacity = sizeof(frame_buffer)
    };
    const size_t out_capacity = 0;
    uint8_t payload_out[1];
    size_t payload_length = 0;
    bool result = hdlc_decode(&frame, payload_out, out_capacity, &payload_length);
    TEST_ASSERT_FALSE(result);
}

void test_decode_frame_too_long(void){
    // Frame: 7E 01 02 03 AD AD 7E
    uint8_t frame_buffer[] = {HDLC_FLAG_BYTE, 0x01, 0x02, 0x03, 0xAD, 0xAD, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = sizeof(frame_buffer),
        .capacity = sizeof(frame_buffer)
    };
    const size_t out_capacity = sizeof(frame_buffer) -2 -2 -1; //-2 flags -2 crc -1 to underflow
    uint8_t payload_out[out_capacity];
    size_t payload_length = 0;
    bool result = hdlc_decode(&frame, payload_out, out_capacity, &payload_length);
    TEST_ASSERT_FALSE(result);
}

void test_decode_payload_fits_exact_capacity(void){
    // Frame: 7E 01 02 03 AD AD 7E
    uint8_t frame_buffer[] = {HDLC_FLAG_BYTE, 0x01, 0x02, 0x03, 0xAD, 0xAD, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = sizeof(frame_buffer),
        .capacity = sizeof(frame_buffer)
    };
    const size_t out_capacity = 3+2;
    uint8_t payload_out[out_capacity];
    size_t payload_length = 0;
    bool result = hdlc_decode(&frame, payload_out, out_capacity, &payload_length);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(3, payload_length);
    TEST_ASSERT_EQUAL_HEX8(0x01, payload_out[0]);
    TEST_ASSERT_EQUAL_HEX8(0x02, payload_out[1]);
    TEST_ASSERT_EQUAL_HEX8(0x03, payload_out[2]);
}

void test_decode_one_byte_flag_escape(void){
    // Frame: 7E 7D 5E 7D 5E A9 7E
    uint8_t frame_buffer[] = {HDLC_FLAG_BYTE, HDLC_ESCAPE_BYTE, 0x5E, HDLC_ESCAPE_BYTE, 0x5E, 0xA9, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = sizeof(frame_buffer),
        .capacity = sizeof(frame_buffer)
    };
    const size_t out_capacity = 10;
    uint8_t payload_out[out_capacity];
    size_t payload_length = 0;
    bool result = hdlc_decode(&frame, payload_out, out_capacity, &payload_length);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, payload_length);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, payload_out[0]);
}


void test_decode_second_byte_flag_escape(void){
    // Frame: 7E 42 7D 5E E9 F8 7E
    uint8_t frame_buffer[] = {HDLC_FLAG_BYTE, 0x42, HDLC_ESCAPE_BYTE, 0x5E, 0xE9, 0xF8, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = sizeof(frame_buffer),
        .capacity = sizeof(frame_buffer)
    };
    const size_t out_capacity = 2+2;
    uint8_t payload_out[out_capacity];
    size_t payload_length = 0;
    bool result = hdlc_decode(&frame, payload_out, out_capacity, &payload_length);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(2, payload_length);
    TEST_ASSERT_EQUAL_HEX8(0x42, payload_out[0]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, payload_out[1]);
}

void test_decode_one_byte_escape_escape(void){
    // Frame: 7E 7D 5D 4E CA 7E
    uint8_t frame_buffer[] = {HDLC_FLAG_BYTE, HDLC_ESCAPE_BYTE, 0x5D, 0x4E, 0xCA, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = sizeof(frame_buffer),
        .capacity = sizeof(frame_buffer)
    };
    const size_t out_capacity = 10;
    uint8_t payload_out[out_capacity];
    size_t payload_length = 0;
    bool result = hdlc_decode(&frame, payload_out, out_capacity, &payload_length);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, payload_length);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE, payload_out[0]);
}


void test_decode_second_byte_escape_escape(void){
    // Frame: 7E 42 7D 5D D9 9B 7E
    uint8_t frame_buffer[] = {HDLC_FLAG_BYTE, 0x42, HDLC_ESCAPE_BYTE, 0x5D, 0xD9, 0x9B, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = sizeof(frame_buffer),
        .capacity = sizeof(frame_buffer)
    };
    const size_t out_capacity = 2+2;
    uint8_t payload_out[out_capacity];
    size_t payload_length = 0;
    bool result = hdlc_decode(&frame, payload_out, out_capacity, &payload_length);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(2, payload_length);
    TEST_ASSERT_EQUAL_HEX8(0x42, payload_out[0]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE, payload_out[1]);
}

void test_decode_mixed_escape_and_plain_bytes(void){
    // Frame: 7E 11 7D 5E 22 7D 5D 33 CF BB 7E
    uint8_t frame_buffer[] = {HDLC_FLAG_BYTE, 0x11, HDLC_ESCAPE_BYTE,
        0x5E, 0x22, HDLC_ESCAPE_BYTE, 0x5D, 0x33, 0xCF, 0xBB, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = sizeof(frame_buffer),
        .capacity = sizeof(frame_buffer)
    };
    const size_t out_capacity = 5+2;
    uint8_t payload_out[out_capacity];
    size_t payload_length = 0;
    bool result = hdlc_decode(&frame, payload_out, out_capacity, &payload_length);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(5, payload_length);
    // 0x11, HDLC_FLAG_BYTE, 0x22, HDLC_ESCAPE_BYTE, 0x33
    TEST_ASSERT_EQUAL_HEX8(0x11, payload_out[0]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, payload_out[1]);
    TEST_ASSERT_EQUAL_HEX8(0x22, payload_out[2]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE, payload_out[3]);
    TEST_ASSERT_EQUAL_HEX8(0x33, payload_out[4]);
}

void test_decode_one_byte_crc_check_sunny_day(void){
    // Frame: 7E 01 F1 D1 7E
    uint8_t frame_buffer[] = {HDLC_FLAG_BYTE, 0x01, 0xF1, 0xD1, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = sizeof(frame_buffer),
        .capacity = sizeof(frame_buffer)
    };
    const size_t out_capacity = 10;
    uint8_t payload_out[out_capacity];
    size_t payload_length = 0;
    bool result = hdlc_decode(&frame, payload_out, out_capacity, &payload_length);
    TEST_ASSERT_TRUE(result);
}


void test_decode_one_byte_crc_check_rainy_day(void){
    // Frame: 7E 01 F1 D1 7E
    uint8_t frame_buffer[] = {HDLC_FLAG_BYTE, 0x02, 0xF1, 0xD1, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = sizeof(frame_buffer),
        .capacity = sizeof(frame_buffer)
    };
    const size_t out_capacity = 10;
    uint8_t payload_out[out_capacity];
    size_t payload_length = 0;
    bool result = hdlc_decode(&frame, payload_out, out_capacity, &payload_length);
    TEST_ASSERT_FALSE(result);
}

void test_decode_one_byte_crc_contains_flag_byte(void){
    // Frame: 7E 4A 08 7D 5E 7E
    uint8_t frame_buffer[] = {HDLC_FLAG_BYTE, 0x4A, 0x08, HDLC_ESCAPE_BYTE, 0x5E, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = sizeof(frame_buffer),
        .capacity = sizeof(frame_buffer)
    };
    const size_t out_capacity = 10;
    uint8_t payload_out[out_capacity];
    size_t payload_length = 0;
    bool result = hdlc_decode(&frame, payload_out, out_capacity, &payload_length);
    TEST_ASSERT_TRUE(result);
}

void test_decode_one_byte_crc_contains_escape_byte(void){
    // Frame: 7E 2F 34 7D 5D 7E
    uint8_t frame_buffer[] = {HDLC_FLAG_BYTE, 0x2F, 0x34, HDLC_ESCAPE_BYTE, 0x5D, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = sizeof(frame_buffer),
        .capacity = sizeof(frame_buffer)
    };
    const size_t out_capacity = 10;
    uint8_t payload_out[out_capacity];
    size_t payload_length = 0;
    bool result = hdlc_decode(&frame, payload_out, out_capacity, &payload_length);
    TEST_ASSERT_TRUE(result);
}

void test_decode_one_byte_crc_contains_escape_and_flag_byte(void){
    // Frame: 7E 39 F3 7D 5D 7D 5E 7E
    uint8_t frame_buffer[] = {HDLC_FLAG_BYTE, 0x39, 0xF3, HDLC_ESCAPE_BYTE, 0x5D, HDLC_ESCAPE_BYTE, 0x5E, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = sizeof(frame_buffer),
        .capacity = sizeof(frame_buffer)
    };
    const size_t out_capacity = 10;
    uint8_t payload_out[out_capacity];
    size_t payload_length = 0;
    bool result = hdlc_decode(&frame, payload_out, out_capacity, &payload_length);
    TEST_ASSERT_TRUE(result);
}

void test_round_trip(void){
    uint8_t payload[] =  {
    0x0E, 0x30, 0x2C, 0xD3, 0xDD, 0x7E, 0xFD, 0x9F, 0x9E, 0xF3,
    0xB9, 0x22, 0x52, 0xDB, 0x83, 0xEE, 0x3B, 0x4B, 0x02, 0x44,
    0xC4, 0x07, 0x28, 0xBD, 0x99, 0xF6, 0xDA, 0x7D, 0x43, 0x18,
    0x09, 0xB0, 0xCA, 0x92, 0x46, 0x9B, 0x08, 0xAD, 0xA5, 0xE0
    };
    uint8_t frame_buffer[128];

    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };

    bool result = hdlc_encode(payload, sizeof(payload), &frame);
    TEST_ASSERT_TRUE(result);

    const size_t out_capacity = 256;
    uint8_t payload_out[out_capacity];
    size_t payload_length = 0;
    result = hdlc_decode(&frame, payload_out, out_capacity, &payload_length);
    TEST_ASSERT_TRUE(result);

    for(size_t i = 0; i < sizeof(payload); i++){
        TEST_ASSERT_EQUAL_HEX8(payload[i], payload_out[i]);
    }
}
