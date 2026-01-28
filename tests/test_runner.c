#include "unity.h"
#include <stdint.h>

extern void test_encode_empty_frame_no_escape(void);
extern void test_encode_one_byte_frame_no_escape(void);
extern void test_encode_three_bytes_frame_no_escape(void);
extern void test_encode_abort_empty_frame(void);
extern void test_encode_abort_payload_null_but_length(void);
extern void test_encode_abort_payload_too_long(void);
extern void test_encode_abort_frame_capacity_invalid(void);
extern void test_encode_payload_fits_exact_capacity(void);
extern void test_encode_one_byte_flag_escape(void);
extern void test_encode_second_byte_flag_escape(void);
extern void test_encode_one_byte_escape_escape(void);
extern void test_encode_second_byte_escape_escape(void);
extern void test_encode_mixed_escape_and_plain_bytes(void);
extern void test_encode_buffer_of_due_to_escape(void);
extern void test_encode_one_byte_crc_check(void);
extern void test_encode_one_byte_crc_contains_flag_byte(void);
extern void test_encode_one_byte_crc_contains_escape_byte(void);
extern void test_encode_one_byte_crc_contains_escape_and_flag_byte(void);

extern void test_decode_empty_frame_no_escape(void);
extern void test_decode_one_byte_frame_no_escape(void);
extern void test_decode_three_bytes_frame_no_escape(void);
extern void test_decode_abort_empty_frame(void);
extern void test_decode_abort_empty_frame_2(void);
extern void test_decode_abort_payload_null(void);
extern void test_decode_abort_no_capacity(void);
extern void test_decode_frame_too_long(void);
extern void test_decode_payload_fits_exact_capacity(void);
extern void test_decode_one_byte_flag_escape(void);
extern void test_decode_second_byte_flag_escape(void);
extern void test_decode_one_byte_escape_escape(void);
extern void test_decode_mixed_escape_and_plain_bytes(void);
extern void test_decode_one_byte_crc_check_sunny_day(void);
extern void test_decode_one_byte_crc_check_rainy_day(void);
extern void test_decode_one_byte_crc_contains_flag_byte(void);
extern void test_decode_one_byte_crc_contains_escape_byte(void);
extern void test_decode_one_byte_crc_contains_escape_and_flag_byte(void);
extern void test_round_trip(void);

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

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
    RUN_TEST(test_encode_one_byte_crc_check);
    RUN_TEST(test_encode_one_byte_crc_contains_flag_byte);
    RUN_TEST(test_encode_one_byte_crc_contains_escape_byte);
    RUN_TEST(test_encode_one_byte_crc_contains_escape_and_flag_byte);

    RUN_TEST(test_decode_empty_frame_no_escape);
    RUN_TEST(test_decode_one_byte_frame_no_escape);
    RUN_TEST(test_decode_three_bytes_frame_no_escape);
    RUN_TEST(test_decode_abort_empty_frame);
    RUN_TEST(test_decode_abort_empty_frame_2);
    RUN_TEST(test_decode_abort_payload_null);
    RUN_TEST(test_decode_abort_no_capacity);
    RUN_TEST(test_decode_frame_too_long);
    RUN_TEST(test_decode_payload_fits_exact_capacity);
    RUN_TEST(test_decode_one_byte_flag_escape);
    RUN_TEST(test_decode_second_byte_flag_escape);
    RUN_TEST(test_decode_one_byte_escape_escape);
    RUN_TEST(test_decode_mixed_escape_and_plain_bytes);
    RUN_TEST(test_decode_one_byte_crc_check_sunny_day);
    RUN_TEST(test_decode_one_byte_crc_check_rainy_day);
    RUN_TEST(test_decode_one_byte_crc_contains_flag_byte);
    RUN_TEST(test_decode_one_byte_crc_contains_escape_byte);
    RUN_TEST(test_decode_one_byte_crc_contains_escape_and_flag_byte);
    RUN_TEST(test_round_trip);
    return UNITY_END();
}
