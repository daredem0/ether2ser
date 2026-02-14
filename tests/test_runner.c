#include "unity.h"
#include <stdint.h>
// Keep this test TU build-only (no CMake changes required)
#include "test_hdlc_bitstuff_decoder.c"

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
extern void test_bitstuff_encoder_roundtrip_lsb_small(void);
extern void test_bitstuff_encoder_roundtrip_lsb_stuffing(void);
extern void test_bitstuff_encoder_roundtrip_msb(void);
extern void test_bitstuff_encoder_empty_payload(void);
extern void test_bitstuff_encoder_one_byte_payload(void);
extern void test_bitstuff_encoder_payload_with_flag_and_escape_bytes(void);
extern void test_bitstuff_encoder_stuffing_count_lsb(void);
extern void test_bitstuff_encoder_abort_payload_null_with_length(void);
extern void test_bitstuff_encoder_abort_frame_null(void);
extern void test_bitstuff_encoder_abort_frame_capacity_invalid(void);
extern void test_bitstuff_encoder_abort_frame_length_nonzero(void);

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
extern void test_bitstuff_decode_roundtrip_empty_lsb(void);
extern void test_bitstuff_decode_roundtrip_one_byte_lsb(void);
extern void test_bitstuff_decode_roundtrip_multi_byte_lsb(void);
extern void test_bitstuff_decode_roundtrip_multi_byte_msb(void);
extern void test_bitstuff_decode_roundtrip_large_payload(void);
extern void test_bitstuff_decode_abort_null_frame(void);
extern void test_bitstuff_decode_abort_null_payload(void);
extern void test_bitstuff_decode_abort_null_payload_length(void);
extern void test_bitstuff_decode_abort_zero_out_capacity(void);
extern void test_bitstuff_decode_abort_missing_start_flag(void);
extern void test_bitstuff_decode_abort_missing_end_flag(void);
extern void test_bitstuff_decode_abort_payload_too_long(void);
extern void test_bitstuff_decode_exact_capacity_succeeds(void);
extern void test_bitstuff_decode_abort_crc_mismatch(void);
extern void test_bitstuff_decode_abort_unstuff_violation(void);
extern void test_bitstuff_decode_abort_bit_order_mismatch(void);

// CLI Parser declarations
extern void test_cli_parse_command_only(void);
extern void test_cli_parse_command_and_args(void);
extern void test_cli_parse_empty_line(void);
extern void test_cli_parse_space_only_line(void);
extern void test_cli_parse_leading_spaces(void);
extern void test_cli_parse_cmd_truncation(void);
extern void test_cli_parse_args_truncation(void);
extern void test_cli_parse_exact_capacity_cmd_and_args(void);
extern void test_cli_parse_null_params(void);
extern void test_cli_parse_zero_capacity(void);
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
extern void test_parse_set_ip_args_plain_ok(void);
extern void test_parse_set_ip_args_with_ip_prefix_ok(void);
extern void test_parse_set_ip_args_invalid_values_fail(void);
extern void test_parse_set_net_ip_args_ok_and_prefix_required(void);
extern void test_parse_set_ip_remote_args_ok_and_fail(void);
extern void test_parse_set_gateway_args_ok_and_fail(void);
extern void test_parse_set_udp_port_local_args_ok_and_fail(void);
extern void test_parse_set_udp_port_remote_args_ok_and_fail(void);
extern void test_parse_set_v24_polarities_empty_or_null_args_ok(void);
extern void test_parse_set_v24_polarities_tokens_ok(void);
extern void test_parse_set_v24_polarities_invalid_token_fails(void);
extern void test_parse_set_v24_polarities_null_output_fails(void);
extern void test_parse_set_v24_baudrate_valid_and_invalid(void);

// Event Queue declarations
extern void test_event_queue_empty_after_init(void);
extern void test_event_queue_push_pop_single(void);
extern void test_event_queue_full_when_capacity_reached(void);
extern void test_event_queue_pop_empty_fails(void);
extern void test_event_queue_wraparound_preserves_order(void);
extern void test_event_get_payload_ptr_inline_ok(void);
extern void test_event_get_payload_ptr_inline_too_small_fails(void);
extern void test_event_get_payload_ptr_pointer_ok(void);
extern void test_event_get_payload_ptr_pointer_invalid_fails(void);
extern void test_event_get_payload_ptr_invalid_args_fail(void);

// Frame Accumulator declarations
extern void test_byte_aligned_hdlc_frame(void);
extern void test_one_bit_shifted_hdlc_frame(void);
extern void test_multiple_bits_shifted_hdlc_frame(void);
extern void test_hdlc_sync_incomplete_frame_not_ready(void);
extern void test_hdlc_sync_noise_then_frame(void);
extern void test_hdlc_sync_accumulator_overflow_rejected(void);
extern void test_hdlc_sync_poll_invalid_args_returns_ok(void);
extern void test_hdlc_sync_poll_syncing_waits_for_lookahead(void);
extern void test_hdlc_sync_poll_synced_waits_for_lookahead(void);
extern void test_hdlc_sync_poll_overflow_resets_state(void);
extern void test_hdlc_sync_consume_candidate_drops_prefix_and_resets(void);
extern void test_hdlc_sync_consume_candidate_applies_hardcap(void);
extern void test_hdlc_sync_tck_long_trace_decodes_ten_1472b_frames_with_sequence(void);
extern void test_hdlc_sync_xck_long4_trace_decodes_ten_1472b_frames_with_sequence(void);

// Ringbuffer declarations
extern void test_ringbuffer_init_null_buffer(void);
extern void test_ringbuffer_init_sets_state(void);
extern void test_ringbuffer_push_pop_order_with_wrap(void);
extern void test_ringbuffer_push_full_fails(void);
extern void test_ringbuffer_pop_empty_fails(void);
extern void test_ringbuffer_push_wrap_overwrites_oldest(void);
extern void test_ringbuffer_uint16_items_preserve_values_and_wrap(void);
extern void test_ringbuffer_push_wrap_overwrites_oldest_for_uint16_items(void);

// Log tests
extern void test_log_set_get_level_roundtrip(void);
extern void test_log_filtered_message_does_not_set_emitted_flag(void);
extern void test_log_emitted_message_sets_and_clears_flag(void);
extern void test_log_trace_visible_only_at_trace_level(void);


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
    RUN_TEST(test_bitstuff_encoder_roundtrip_lsb_small);
    RUN_TEST(test_bitstuff_encoder_roundtrip_lsb_stuffing);
    RUN_TEST(test_bitstuff_encoder_roundtrip_msb);
    RUN_TEST(test_bitstuff_encoder_empty_payload);
    RUN_TEST(test_bitstuff_encoder_one_byte_payload);
    RUN_TEST(test_bitstuff_encoder_payload_with_flag_and_escape_bytes);
    RUN_TEST(test_bitstuff_encoder_stuffing_count_lsb);
    RUN_TEST(test_bitstuff_encoder_abort_payload_null_with_length);
    RUN_TEST(test_bitstuff_encoder_abort_frame_null);
    RUN_TEST(test_bitstuff_encoder_abort_frame_capacity_invalid);
    RUN_TEST(test_bitstuff_encoder_abort_frame_length_nonzero);

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
    RUN_TEST(test_bitstuff_decode_roundtrip_empty_lsb);
    RUN_TEST(test_bitstuff_decode_roundtrip_one_byte_lsb);
    RUN_TEST(test_bitstuff_decode_roundtrip_multi_byte_lsb);
    RUN_TEST(test_bitstuff_decode_roundtrip_multi_byte_msb);
    RUN_TEST(test_bitstuff_decode_roundtrip_large_payload);
    RUN_TEST(test_bitstuff_decode_abort_null_frame);
    RUN_TEST(test_bitstuff_decode_abort_null_payload);
    RUN_TEST(test_bitstuff_decode_abort_null_payload_length);
    RUN_TEST(test_bitstuff_decode_abort_zero_out_capacity);
    RUN_TEST(test_bitstuff_decode_abort_missing_start_flag);
    RUN_TEST(test_bitstuff_decode_abort_missing_end_flag);
    RUN_TEST(test_bitstuff_decode_abort_payload_too_long);
    RUN_TEST(test_bitstuff_decode_exact_capacity_succeeds);
    RUN_TEST(test_bitstuff_decode_abort_crc_mismatch);
    RUN_TEST(test_bitstuff_decode_abort_unstuff_violation);
    RUN_TEST(test_bitstuff_decode_abort_bit_order_mismatch);

    // CLI Parser Tests
    RUN_TEST(test_cli_parse_command_only);
    RUN_TEST(test_cli_parse_command_and_args);
    RUN_TEST(test_cli_parse_empty_line);
    RUN_TEST(test_cli_parse_space_only_line);
    RUN_TEST(test_cli_parse_leading_spaces);
    RUN_TEST(test_cli_parse_cmd_truncation);
    RUN_TEST(test_cli_parse_args_truncation);
    RUN_TEST(test_cli_parse_exact_capacity_cmd_and_args);
    RUN_TEST(test_cli_parse_null_params);
    RUN_TEST(test_cli_parse_zero_capacity);
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
    RUN_TEST(test_parse_set_ip_args_plain_ok);
    RUN_TEST(test_parse_set_ip_args_with_ip_prefix_ok);
    RUN_TEST(test_parse_set_ip_args_invalid_values_fail);
    RUN_TEST(test_parse_set_net_ip_args_ok_and_prefix_required);
    RUN_TEST(test_parse_set_ip_remote_args_ok_and_fail);
    RUN_TEST(test_parse_set_gateway_args_ok_and_fail);
    RUN_TEST(test_parse_set_udp_port_local_args_ok_and_fail);
    RUN_TEST(test_parse_set_udp_port_remote_args_ok_and_fail);
    RUN_TEST(test_parse_set_v24_polarities_empty_or_null_args_ok);
    RUN_TEST(test_parse_set_v24_polarities_tokens_ok);
    RUN_TEST(test_parse_set_v24_polarities_invalid_token_fails);
    RUN_TEST(test_parse_set_v24_polarities_null_output_fails);
    RUN_TEST(test_parse_set_v24_baudrate_valid_and_invalid);

    // Event Queue Tests
    RUN_TEST(test_event_queue_empty_after_init);
    RUN_TEST(test_event_queue_push_pop_single);
    RUN_TEST(test_event_queue_full_when_capacity_reached);
    RUN_TEST(test_event_queue_pop_empty_fails);
    RUN_TEST(test_event_queue_wraparound_preserves_order);
    RUN_TEST(test_event_get_payload_ptr_inline_ok);
    RUN_TEST(test_event_get_payload_ptr_inline_too_small_fails);
    RUN_TEST(test_event_get_payload_ptr_pointer_ok);
    RUN_TEST(test_event_get_payload_ptr_pointer_invalid_fails);
    RUN_TEST(test_event_get_payload_ptr_invalid_args_fail);

    // Frame Accumulator Tests
    RUN_TEST(test_byte_aligned_hdlc_frame);
    RUN_TEST(test_one_bit_shifted_hdlc_frame);
    RUN_TEST(test_multiple_bits_shifted_hdlc_frame);
    RUN_TEST(test_hdlc_sync_incomplete_frame_not_ready);
    RUN_TEST(test_hdlc_sync_noise_then_frame);
    RUN_TEST(test_hdlc_sync_accumulator_overflow_rejected);
    RUN_TEST(test_hdlc_sync_poll_invalid_args_returns_ok);
    RUN_TEST(test_hdlc_sync_poll_syncing_waits_for_lookahead);
    RUN_TEST(test_hdlc_sync_poll_synced_waits_for_lookahead);
    RUN_TEST(test_hdlc_sync_poll_overflow_resets_state);
    RUN_TEST(test_hdlc_sync_consume_candidate_drops_prefix_and_resets);
    RUN_TEST(test_hdlc_sync_consume_candidate_applies_hardcap);
    RUN_TEST(test_hdlc_sync_tck_long_trace_decodes_ten_1472b_frames_with_sequence);
    RUN_TEST(test_hdlc_sync_xck_long4_trace_decodes_ten_1472b_frames_with_sequence);

    // Ringbuffer Tests
    RUN_TEST(test_ringbuffer_init_null_buffer);
    RUN_TEST(test_ringbuffer_init_sets_state);
    RUN_TEST(test_ringbuffer_push_pop_order_with_wrap);
    RUN_TEST(test_ringbuffer_push_full_fails);
    RUN_TEST(test_ringbuffer_pop_empty_fails);
    RUN_TEST(test_ringbuffer_push_wrap_overwrites_oldest);
    RUN_TEST(test_ringbuffer_uint16_items_preserve_values_and_wrap);
    RUN_TEST(test_ringbuffer_push_wrap_overwrites_oldest_for_uint16_items);

    // Log Tests
    RUN_TEST(test_log_set_get_level_roundtrip);
    RUN_TEST(test_log_filtered_message_does_not_set_emitted_flag);
    RUN_TEST(test_log_emitted_message_sets_and_clears_flag);
    RUN_TEST(test_log_trace_visible_only_at_trace_level);

    return UNITY_END();
}
