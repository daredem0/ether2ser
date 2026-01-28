#include "unity.h"
#include <stdint.h>
#include "protocol/hdlc_encoder.h"

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_encode_empty_frame_no_escape(void){
    uint8_t frame_buffer[10];
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };

    bool result = hdlc_encode(NULL, 0, &frame);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(2, frame.length);
    TEST_ASSERT_EQUAL_HEX8(0x7E, frame.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x7E, frame.payload[1]);
}

void test_encode_one_byte_frame_no_escape(void){
    uint8_t payload[] = {0x42}; // single byte that should not be escaped
    uint8_t frame_buffer[10];
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };

    bool result = hdlc_encode(payload, 1, &frame);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(3, frame.length);
    TEST_ASSERT_EQUAL_HEX8(0x7E, frame.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x42, frame.payload[1]);
    TEST_ASSERT_EQUAL_HEX8(0x7E, frame.payload[2]);
}

void test_encode_three_bytes_frame_no_escape(void){
    uint8_t frame_buffer[16];
    uint8_t payload[] = {0x01, 0x02, 0x03}; // three bytes that should not be escaped
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };
    bool result = hdlc_encode(payload, 3, &frame);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(5, frame.length);
    TEST_ASSERT_EQUAL_HEX8(0x7E, frame.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x01, frame.payload[1]);
    TEST_ASSERT_EQUAL_HEX8(0x02, frame.payload[2]);
    TEST_ASSERT_EQUAL_HEX8(0x03, frame.payload[3]);
    TEST_ASSERT_EQUAL_HEX8(0x7E, frame.payload[4]);

}

void test_encode_abort_empty_frame(void){
    uint8_t payload[] = {0x01};
    bool result = hdlc_encode(payload, 0, NULL);
    TEST_ASSERT_FALSE(result);
}

void test_encode_abort_payload_null_but_length(void){
    int8_t frame_buffer[16];
    uint8_t payload[] = {0x01, 0x02, 0x03}; // three bytes that should not be escaped
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };
    bool result = hdlc_encode(NULL, 1, &frame);
    TEST_ASSERT_FALSE(result);
}

void test_encode_abort_payload_too_long(void){
    int8_t frame_buffer[16];
    uint8_t payload[] = {0x01, 0x02, 0x03}; // three bytes that should not be escaped
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = 2
    };
    bool result = hdlc_encode(payload, 4, &frame);
    TEST_ASSERT_FALSE(result);
}

void test_encode_abort_frame_capacity_invalid(void){
    int8_t frame_buffer[16];
    uint8_t payload[] = {0x01, 0x02, 0x03}; // three bytes that should not be escaped
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = 1
    };
    bool result = hdlc_encode(payload, 4, &frame);
    TEST_ASSERT_FALSE(result);
}

void test_encode_payload_fits_exact_capacity(void){
    uint8_t frame_buffer[5]; // 2 flags + 3 payload
    uint8_t payload[] = {0x01, 0x02, 0x03};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };

    bool result = hdlc_encode(payload, 3, &frame);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(5, frame.length);
    TEST_ASSERT_EQUAL_HEX8(0x7E, frame.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x01, frame.payload[1]);
    TEST_ASSERT_EQUAL_HEX8(0x02, frame.payload[2]);
    TEST_ASSERT_EQUAL_HEX8(0x03, frame.payload[3]);
    TEST_ASSERT_EQUAL_HEX8(0x7E, frame.payload[4]);
}

void test_encode_one_byte_flag_escape(void){
    uint8_t frame_buffer[5]; // 2 flags + 1 payload + 1 escape
    uint8_t payload[] = {HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };
    bool result = hdlc_encode(payload, 1, &frame);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(4, frame.length);
    TEST_ASSERT_EQUAL_HEX8(0x7E, frame.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE, frame.payload[1]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE^HDLC_ESCAPE_XOR, frame.payload[2]);
    TEST_ASSERT_EQUAL_HEX8(0x7E, frame.payload[3]);
}

void test_encode_second_byte_flag_escape(void){
    uint8_t frame_buffer[6]; // 2 flags + 2 payload + 1 escape
    uint8_t payload[] = {0x42, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };
    bool result = hdlc_encode(payload, 2, &frame);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(5, frame.length);
    TEST_ASSERT_EQUAL_HEX8(0x7E, frame.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x42, frame.payload[1]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE, frame.payload[2]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE^HDLC_ESCAPE_XOR, frame.payload[3]);
    TEST_ASSERT_EQUAL_HEX8(0x7E, frame.payload[4]);
}

void test_encode_one_byte_escape_escape(void){
    uint8_t frame_buffer[5]; // 2 flags + 1 payload + 1 escape
    uint8_t payload[] = {HDLC_ESCAPE_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };
    bool result = hdlc_encode(payload, 1, &frame);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(4, frame.length);
    TEST_ASSERT_EQUAL_HEX8(0x7E, frame.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE, frame.payload[1]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE^HDLC_ESCAPE_XOR, frame.payload[2]);
    TEST_ASSERT_EQUAL_HEX8(0x7E, frame.payload[3]);
}

void test_encode_second_byte_escape_escape(void){
    uint8_t frame_buffer[6]; // 2 flags + 2 payload + 1 escape
    uint8_t payload[] = {0x42, HDLC_ESCAPE_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };
    bool result = hdlc_encode(payload, 2, &frame);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(5, frame.length);
    TEST_ASSERT_EQUAL_HEX8(0x7E, frame.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x42, frame.payload[1]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE, frame.payload[2]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE^HDLC_ESCAPE_XOR, frame.payload[3]);
    TEST_ASSERT_EQUAL_HEX8(0x7E, frame.payload[4]);
}

void test_encode_mixed_escape_and_plain_bytes(void){
    uint8_t payload[] = {0x11, HDLC_FLAG_BYTE, 0x22, HDLC_ESCAPE_BYTE, 0x33};
    uint8_t frame_buffer[16];
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };

    bool result = hdlc_encode(payload, sizeof(payload), &frame);
    TEST_ASSERT_TRUE(result);

    // expected: flag + 0x11 + 0x7D 0x5E + 0x22 + 0x7D 0x5D + 0x33 + flag
    TEST_ASSERT_EQUAL(2 + 5 + 2, frame.length); // 2 flags + (5 + 2 escapes)
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x11, frame.payload[1]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE, frame.payload[2]);
    TEST_ASSERT_EQUAL_HEX8(0x5E, frame.payload[3]);
    TEST_ASSERT_EQUAL_HEX8(0x22, frame.payload[4]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE, frame.payload[5]);
    TEST_ASSERT_EQUAL_HEX8(0x5D, frame.payload[6]);
    TEST_ASSERT_EQUAL_HEX8(0x33, frame.payload[7]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[8]);
}

void test_encode_buffer_of_due_to_escape(void){
    uint8_t payload[] = {HDLC_FLAG_BYTE, HDLC_ESCAPE_BYTE};
    uint8_t frame_buffer[5];
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };

    bool result = hdlc_encode(payload, sizeof(payload), &frame);
    TEST_ASSERT_FALSE(result);

    // TEST_ASSERT_EQUAL(2 + 2 + 2, frame.length);
    // TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[0]);
    // TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE, frame.payload[1]);
    // TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE^HDLC_ESCAPE_XOR, frame.payload[2]);
    // TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE, frame.payload[3]);
    // TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE^HDLC_ESCAPE_XOR, frame.payload[4]);
    // TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[5]);
}


// not needed when using generate_test_runner.rb
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_encode_empty_frame_no_escape);
    RUN_TEST(test_encode_one_byte_frame_no_escape);
    RUN_TEST(test_encode_three_bytes_frame_no_escape);
    RUN_TEST(test_encode_abort_empty_frame);
    RUN_TEST(test_encode_abort_payload_null_but_length);
    RUN_TEST(test_encode_abort_payload_too_long);
    RUN_TEST(test_encode_abort_frame_capacity_invalid);
    RUN_TEST(test_encode_payload_fits_exact_capacity);
    RUN_TEST(test_encode_one_byte_flag_escape);
    RUN_TEST(test_encode_second_byte_flag_escape);
    RUN_TEST(test_encode_one_byte_escape_escape);
    RUN_TEST(test_encode_second_byte_escape_escape);
    RUN_TEST(test_encode_mixed_escape_and_plain_bytes);
    RUN_TEST(test_encode_buffer_of_due_to_escape);
    return UNITY_END();
}
