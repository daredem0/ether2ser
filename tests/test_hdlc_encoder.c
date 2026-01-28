#include "unity.h"
#include <stdint.h>
#include "protocol/hdlc_encoder.h"

// void setUp(void) {
//     // set stuff up here
// }

// void tearDown(void) {
//     // clean stuff up here
// }

void test_encode_empty_frame_no_escape(void){
    // Frame: 7E FF FF 7E
    uint8_t frame_buffer[10];
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };

    bool result = hdlc_encode(NULL, 0, &frame);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(4, frame.length);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, frame.payload[1]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, frame.payload[2]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[3]);
}

void test_encode_one_byte_frame_no_escape(void){
    // Frame: 7E 42 89 76 7E
    uint8_t payload[] = {0x42}; // single byte that should not be escaped
    uint8_t frame_buffer[10];
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };

    bool result = hdlc_encode(payload, 1, &frame);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(5, frame.length);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x42, frame.payload[1]);
    TEST_ASSERT_EQUAL_HEX8(0x89, frame.payload[2]);
    TEST_ASSERT_EQUAL_HEX8(0x76, frame.payload[3]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[4]);
}

void test_encode_three_bytes_frame_no_escape(void){
    // Frame: 7E 01 02 03 AD AD 7E
    uint8_t frame_buffer[16];
    uint8_t payload[] = {0x01, 0x02, 0x03}; // three bytes that should not be escaped
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };
    bool result = hdlc_encode(payload, 3, &frame);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(7, frame.length);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x01, frame.payload[1]);
    TEST_ASSERT_EQUAL_HEX8(0x02, frame.payload[2]);
    TEST_ASSERT_EQUAL_HEX8(0x03, frame.payload[3]);
    TEST_ASSERT_EQUAL_HEX8(0xAD, frame.payload[4]);
    TEST_ASSERT_EQUAL_HEX8(0xAD, frame.payload[5]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[6]);

}

void test_encode_abort_empty_frame(void){
    // Frame: <none> (encode fails)
    uint8_t payload[] = {0x01};
    bool result = hdlc_encode(payload, 0, NULL);
    TEST_ASSERT_FALSE(result);
}

void test_encode_abort_payload_null_but_length(void){
    // Frame: <none> (encode fails)
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
    // Frame: <none> (encode fails)
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
    // Frame: <none> (encode fails)
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
    // Frame: 7E 01 02 03 AD AD 7E
    uint8_t frame_buffer[7]; // 2 flags + 3 payload + 2 crc
    uint8_t payload[] = {0x01, 0x02, 0x03};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };

    bool result = hdlc_encode(payload, 3, &frame);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(7, frame.length);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x01, frame.payload[1]);
    TEST_ASSERT_EQUAL_HEX8(0x02, frame.payload[2]);
    TEST_ASSERT_EQUAL_HEX8(0x03, frame.payload[3]);
    TEST_ASSERT_EQUAL_HEX8(0xAD, frame.payload[4]);
    TEST_ASSERT_EQUAL_HEX8(0xAD, frame.payload[5]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[6]);
}

void test_encode_one_byte_flag_escape(void){
    // Frame: 7E 7D 5E 7D 5E A9 7E
    uint8_t frame_buffer[7]; // 2 flags + 1 payload + 1 escape + 2 crc + 1 escape
    uint8_t payload[] = {HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };
    bool result = hdlc_encode(payload, 1, &frame);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(7, frame.length);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE, frame.payload[1]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE^HDLC_ESCAPE_XOR, frame.payload[2]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE, frame.payload[3]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE^HDLC_ESCAPE_XOR, frame.payload[4]);
    TEST_ASSERT_EQUAL_HEX8(0xA9, frame.payload[5]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[6]);
}

void test_encode_second_byte_flag_escape(void){
    // Frame: 7E 42 7D 5E E9 F8 7E
    uint8_t frame_buffer[7]; // 2 flags + 2 payload + 1 escape + 2crc
    uint8_t payload[] = {0x42, HDLC_FLAG_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };
    bool result = hdlc_encode(payload, 2, &frame);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(7, frame.length);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x42, frame.payload[1]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE, frame.payload[2]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE^HDLC_ESCAPE_XOR, frame.payload[3]);
    TEST_ASSERT_EQUAL_HEX8(0xE9, frame.payload[4]);
    TEST_ASSERT_EQUAL_HEX8(0xF8, frame.payload[5]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[6]);
}

void test_encode_one_byte_escape_escape(void){
    // Frame: 7E 7D 5D 4E CA 7E
    uint8_t frame_buffer[6]; // 2 flags + 1 payload + 1 escape + 2 crc
    uint8_t payload[] = {HDLC_ESCAPE_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };
    bool result = hdlc_encode(payload, 1, &frame);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(6, frame.length);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE, frame.payload[1]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE^HDLC_ESCAPE_XOR, frame.payload[2]);
    TEST_ASSERT_EQUAL_HEX8(0x4E, frame.payload[3]);
    TEST_ASSERT_EQUAL_HEX8(0xCA, frame.payload[4]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[5]);
}

void test_encode_second_byte_escape_escape(void){
    // Frame: 7E 42 7D 5D D9 9B 7E
    uint8_t frame_buffer[7]; // 2 flags + 2 payload + 1 escape + 2 crcc
    uint8_t payload[] = {0x42, HDLC_ESCAPE_BYTE};
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };
    bool result = hdlc_encode(payload, 2, &frame);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(7, frame.length);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x42, frame.payload[1]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE, frame.payload[2]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE^HDLC_ESCAPE_XOR, frame.payload[3]);
    TEST_ASSERT_EQUAL_HEX8(0xD9, frame.payload[4]);
    TEST_ASSERT_EQUAL_HEX8(0x9B, frame.payload[5]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[6]);
}

void test_encode_mixed_escape_and_plain_bytes(void){
    // Frame: 7E 11 7D 5E 22 7D 5D 33 CF BB 7E
    uint8_t payload[] = {0x11, HDLC_FLAG_BYTE, 0x22, HDLC_ESCAPE_BYTE, 0x33};
    uint8_t frame_buffer[16];
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };

    bool result = hdlc_encode(payload, sizeof(payload), &frame);
    TEST_ASSERT_TRUE(result);

    // expected: flag + 0x11 + 0x7D 0x5E + 0x22 + 0x7D 0x5D + 0x33 + 0xCF + 0xBB + flag
    TEST_ASSERT_EQUAL(2 + 5 + 2 + 2, frame.length); // 2 flags + (5 + 2 escapes) + 2 crc
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x11, frame.payload[1]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE, frame.payload[2]);
    TEST_ASSERT_EQUAL_HEX8(0x5E, frame.payload[3]);
    TEST_ASSERT_EQUAL_HEX8(0x22, frame.payload[4]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE, frame.payload[5]);
    TEST_ASSERT_EQUAL_HEX8(0x5D, frame.payload[6]);
    TEST_ASSERT_EQUAL_HEX8(0x33, frame.payload[7]);
    TEST_ASSERT_EQUAL_HEX8(0xCF, frame.payload[8]);
    TEST_ASSERT_EQUAL_HEX8(0xBB, frame.payload[9]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[10]);
}

void test_encode_buffer_of_due_to_escape(void){
    // Frame: <none> (encode fails)
    uint8_t payload[] = {HDLC_FLAG_BYTE, HDLC_ESCAPE_BYTE};
    uint8_t frame_buffer[5];
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };

    bool result = hdlc_encode(payload, sizeof(payload), &frame);
    TEST_ASSERT_FALSE(result);

}

void test_encode_one_byte_crc_check(void){
    // Frame: 7E 01 F1 D1 7E
    uint8_t payload[] = {0x01};
    uint8_t frame_buffer[5];
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };

    bool result = hdlc_encode(payload, sizeof(payload), &frame);
    TEST_ASSERT_TRUE(result);

    TEST_ASSERT_EQUAL(5, frame.length);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x01, frame.payload[1]);
    TEST_ASSERT_EQUAL_HEX8(0xF1, frame.payload[2]);
    TEST_ASSERT_EQUAL_HEX8(0xD1, frame.payload[3]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[4]);
}

void test_encode_one_byte_crc_contains_flag_byte(void){
    // Frame: 7E 4A 08 7D 5E 7E
    uint8_t payload[] = {0x4A}; // Should generate crc with flag byte
    uint8_t frame_buffer[6]; // 2 flags + 1 payload + 2 crc + 1 escape crc
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };

    bool result = hdlc_encode(payload, sizeof(payload), &frame);
    TEST_ASSERT_TRUE(result);

    TEST_ASSERT_EQUAL(6, frame.length);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x4A, frame.payload[1]);
    TEST_ASSERT_EQUAL_HEX8(0x08, frame.payload[2]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE, frame.payload[3]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE^HDLC_ESCAPE_XOR, frame.payload[4]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[5]);
}

void test_encode_one_byte_crc_contains_escape_byte(void){
    // Frame: 7E 2F 34 7D 5D 7E
    uint8_t payload[] = {0x2F}; // Should generate crc with escape byte
    uint8_t frame_buffer[6]; // 2 flags + 1 payload + 2 crc + 1 escape crc
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };

    bool result = hdlc_encode(payload, sizeof(payload), &frame);
    TEST_ASSERT_TRUE(result);

    TEST_ASSERT_EQUAL(6, frame.length);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x2F, frame.payload[1]);
    TEST_ASSERT_EQUAL_HEX8(0x34, frame.payload[2]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE, frame.payload[3]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE^HDLC_ESCAPE_XOR, frame.payload[4]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[5]);
}

void test_encode_one_byte_crc_contains_escape_and_flag_byte(void){
    // Frame: 7E 39 F3 7D 5D 7D 5E 7E
    uint8_t payload[] = {0x39, 0xF3}; // Should generate crc with escape and flag byte
    uint8_t frame_buffer[8]; // 2 flags + 2 payload + 2 crc + 2 escape crc
    HDLC_FRAME_T frame = {
        .payload = frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };

    bool result = hdlc_encode(payload, sizeof(payload), &frame);
    TEST_ASSERT_TRUE(result);

    TEST_ASSERT_EQUAL(8, frame.length);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x39, frame.payload[1]);
    TEST_ASSERT_EQUAL_HEX8(0xF3, frame.payload[2]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE, frame.payload[3]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE^HDLC_ESCAPE_XOR, frame.payload[4]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_ESCAPE_BYTE, frame.payload[5]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE^HDLC_ESCAPE_XOR, frame.payload[6]);
    TEST_ASSERT_EQUAL_HEX8(HDLC_FLAG_BYTE, frame.payload[7]);
}
