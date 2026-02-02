#include "unity.h"
#include <stdint.h>

// HDLC Encoder declarations
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

// HDLC Decoder declarations
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

// CLI Parser declarations
extern void test_cli_parse_command_only(void);
extern void test_cli_parse_command_and_args(void);
extern void test_cli_parse_empty_line(void);
extern void test_cli_parse_set_args_unknown_pin(void);
extern void test_cli_parse_set_args_cli_usage_set(void);
extern void test_cli_parse_set_args_input_only(void);
extern void test_cli_parse_get_args_cli_usage_get(void);
extern void test_cli_parse_get_args_cli_unknown_pin(void);
extern void test_cli_parse_set_args_cli_ok(void);
extern void test_cli_parse_get_args_cli_ok(void);
extern void test_cli_parse_set_args_invalid_value_upper_bound(void);
extern void test_cli_parse_set_args_invalid_value_lower_bound(void);
extern void test_cli_parse_set_args_extra_space_at_end(void);
extern void test_cli_parse_set_args_extra_space_between_pin_and_args(void);

// Event Queue declarations
extern void test_event_queue_empty_after_init(void);
extern void test_event_queue_push_pop_single(void);
extern void test_event_queue_full_when_capacity_reached(void);
extern void test_event_queue_pop_empty_fails(void);
extern void test_event_queue_wraparound_preserves_order(void);

// Frame Accumulator declarations
extern void test_byte_aligned_hdlc_frame(void);
extern void test_one_bit_shifted_hdlc_frame(void);
extern void test_multiple_bits_shifted_hdlc_frame(void);

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

int main(void) {
    UNITY_BEGIN();
    // HDLC Encoder Tests
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

    // HDLC Decoder Tests
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

    // CLI Parser Tests
    RUN_TEST(test_cli_parse_command_only);
    RUN_TEST(test_cli_parse_command_and_args);
    RUN_TEST(test_cli_parse_empty_line);
    RUN_TEST(test_cli_parse_set_args_unknown_pin);
    RUN_TEST(test_cli_parse_set_args_cli_usage_set);
    RUN_TEST(test_cli_parse_set_args_input_only);
    RUN_TEST(test_cli_parse_get_args_cli_usage_get);
    RUN_TEST(test_cli_parse_get_args_cli_unknown_pin);
    RUN_TEST(test_cli_parse_set_args_cli_ok);
    RUN_TEST(test_cli_parse_get_args_cli_ok);
    RUN_TEST(test_cli_parse_set_args_invalid_value_upper_bound);
    RUN_TEST(test_cli_parse_set_args_invalid_value_lower_bound);
    RUN_TEST(test_cli_parse_set_args_extra_space_at_end);
    RUN_TEST(test_cli_parse_set_args_extra_space_between_pin_and_args);

    // Event Queue Tests
    RUN_TEST(test_event_queue_empty_after_init);
    RUN_TEST(test_event_queue_push_pop_single);
    RUN_TEST(test_event_queue_full_when_capacity_reached);
    RUN_TEST(test_event_queue_pop_empty_fails);
    RUN_TEST(test_event_queue_wraparound_preserves_order);

    // Frame Accumulator Tests
    RUN_TEST(test_byte_aligned_hdlc_frame);
    RUN_TEST(test_one_bit_shifted_hdlc_frame);
    RUN_TEST(test_multiple_bits_shifted_hdlc_frame);
    return UNITY_END();
}
