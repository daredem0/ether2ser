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
extern void test_bitstuff_decode_captures_first_crc_failure(void);
extern void test_bitstuff_decode_failure_reset_clears_capture(void);


// Operating mode and manual GPIO policy tests
extern void test_operating_mode_defaults_and_invalid_storage_use_bridge(void);
extern void test_operating_mode_restores_gpio_from_storage(void);
extern void test_operating_mode_change_requires_idle_pipeline(void);
extern void test_bridge_mode_rejects_every_manual_gpio_write(void);
extern void test_gpio_mode_allows_only_nonreserved_outputs(void);
extern void test_management_get_version_returns_runtime_version(void);
extern void test_gpio_catalog_includes_external_clock_and_reserved_reset(void);

// Transport-neutral management tests
extern void test_management_catalog_paths_are_unique(void);
extern void test_management_catalog_exposes_common_mode_and_status_resources(void);
extern void test_management_get_mode_returns_runtime_mode(void);
extern void test_management_set_mode_prepares_persisted_reboot_effect(void);
extern void test_management_set_mode_rejects_busy_pipeline(void);
extern void test_management_set_mode_is_idempotent(void);
extern void test_management_gpio_write_uses_common_mode_policy(void);
extern void test_management_gpio_write_rejects_input_and_reserved_signals(void);
extern void test_management_local_ip_is_accepted_without_forcing_reboot(void);
extern void test_management_reset_stats_prepares_runtime_effect(void);

// Query-independent runtime metrics tests
extern void test_runtime_metrics_first_sample_establishes_baseline(void);
extern void test_runtime_metrics_updates_after_fixed_interval(void);
extern void test_runtime_metrics_snapshot_is_observational(void);
extern void test_runtime_metrics_ignores_subinterval_polling(void);

// Streamlined serial management tests
extern void test_management_cli_parses_get_status(void);
extern void test_management_cli_parses_semantic_mode_and_clock_values(void);
extern void test_management_cli_parses_gpio_path_and_level(void);
extern void test_management_cli_parses_actions_and_wipe_confirmation(void);
extern void test_management_cli_rejects_unknown_trailing_and_oversized_lines(void);
extern void test_management_cli_parses_reset_stats_action(void);
extern void test_management_cli_formats_framed_success_and_error(void);
extern void test_management_cli_formats_inverted_signals_for_humans(void);
extern void test_management_cli_exposes_firmware_version(void);
extern void test_management_cli_human_commands_keep_original_shortcuts(void);
extern void test_management_cli_human_commands_accept_original_category_syntax(void);
extern void test_management_cli_help_is_human_readable(void);
extern void test_management_cli_help_provides_targeted_usage(void);
extern void test_management_http_exposes_firmware_version(void);

// HTTP REST management tests
extern void test_management_http_maps_get_and_nested_paths(void);
extern void test_management_http_maps_put_values_through_common_request_semantics(void);
extern void test_management_http_maps_actions_and_enforces_wipe_confirmation(void);
extern void test_management_http_maps_reset_stats_action(void);
extern void test_management_http_rejects_incomplete_oversized_and_unsupported_requests(void);
extern void test_management_http_formats_bounded_json_success_and_error(void);
extern void test_management_http_server_opens_socket_two_nonblocking_on_port_80(void);
extern void test_management_http_server_listens_without_blocking(void);
extern void test_management_http_server_accumulates_fragments_and_does_one_io_per_poll(void);
extern void test_management_http_server_sends_incrementally_then_applies_effect(void);
extern void test_management_http_formats_shared_status_and_config_snapshots(void);

// Event Queue declarations
extern void test_event_queue_statistics_reset_preserves_pending_events(void);
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
extern void test_hdlc_sync_statistics_reset_preserves_accumulator_state(void);
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

// TX queue tests
extern void test_tx_queue_init_empty(void);
extern void test_tx_queue_enqueue_single(void);
extern void test_tx_queue_full_rejected(void);
extern void test_tx_queue_oversized_payload_rejected(void);
extern void test_tx_queue_restarts_midframe_after_cts_edge(void);
extern void test_tx_queue_protects_queued_tail_until_pio_completion(void);
extern void test_tx_queue_waits_for_rts_release_between_queued_frames(void);
extern void test_tx_queue_statistics_reset_preserves_queued_frames(void);

// Event scheduler tests
extern void test_event_scheduler_blocks_control_during_active_frame(void);
extern void test_event_scheduler_services_boundary_with_backlog(void);
extern void test_event_scheduler_services_full_idle_budget(void);
extern void test_event_scheduler_blocks_background_io_during_active_frame(void);
extern void test_event_scheduler_allows_background_io_at_frame_boundary(void);

// RX hunting idle filter tests
extern void test_rx_hunt_filter_drops_only_redundant_preframe_idle(void);
extern void test_rx_hunt_filter_preserves_repeated_zeroes_after_opening_flag(void);
extern void test_rx_hunt_filter_inactive_states_never_drop(void);
extern void test_tx_queue_active_frame_covers_physical_completion(void);
extern void test_tx_queue_active_frame_clears_before_next_queued_entry(void);

// V.24 TX state tests
extern void test_v24_tx_state_asserts_rts_and_waits_for_settle(void);
extern void test_v24_tx_state_ignores_cts_edges_before_first_byte(void);
extern void test_v24_tx_state_restarts_on_cts_edge_while_sending(void);
extern void test_v24_tx_state_restarts_on_cts_edge_while_draining(void);
extern void test_v24_tx_state_ignores_cts_restart_when_disabled(void);
extern void test_v24_tx_state_restarts_on_fifo_underrun_during_poll(void);
extern void test_v24_tx_state_restarts_on_fifo_underrun_before_write(void);
extern void test_v24_tx_state_completes_after_rts_and_interframe_holds(void);
extern void test_v24_tx_state_reconfigure_resets_lifecycle_and_preserves_restart(void);

// W5500 socket I/O tests
extern void test_w5500_udp_tx_accepts_exact_length_send(void);
extern void test_w5500_udp_tx_rejects_negative_send(void);
extern void test_w5500_udp_tx_rejects_short_send(void);
extern void test_w5500_udp_tx_rejects_invalid_frame(void);

// MAC address tests
extern void test_mac_address_from_ipv4_encodes_unique_local_unicast_address(void);
extern void test_mac_address_from_ipv4_distinguishes_configured_board_ips(void);

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
    RUN_TEST(test_bitstuff_decode_captures_first_crc_failure);
    RUN_TEST(test_bitstuff_decode_failure_reset_clears_capture);


    // Operating mode and manual GPIO policy tests
    RUN_TEST(test_operating_mode_defaults_and_invalid_storage_use_bridge);
    RUN_TEST(test_operating_mode_restores_gpio_from_storage);
    RUN_TEST(test_operating_mode_change_requires_idle_pipeline);
    RUN_TEST(test_bridge_mode_rejects_every_manual_gpio_write);
    RUN_TEST(test_gpio_mode_allows_only_nonreserved_outputs);
    RUN_TEST(test_gpio_catalog_includes_external_clock_and_reserved_reset);

    // Transport-neutral management tests
    RUN_TEST(test_management_catalog_paths_are_unique);
    RUN_TEST(test_management_catalog_exposes_common_mode_and_status_resources);
    RUN_TEST(test_management_get_mode_returns_runtime_mode);
    RUN_TEST(test_management_get_version_returns_runtime_version);
    RUN_TEST(test_management_set_mode_prepares_persisted_reboot_effect);
    RUN_TEST(test_management_set_mode_rejects_busy_pipeline);
    RUN_TEST(test_management_set_mode_is_idempotent);
    RUN_TEST(test_management_gpio_write_uses_common_mode_policy);
    RUN_TEST(test_management_gpio_write_rejects_input_and_reserved_signals);
    RUN_TEST(test_management_local_ip_is_accepted_without_forcing_reboot);
    RUN_TEST(test_management_reset_stats_prepares_runtime_effect);

    // Query-independent runtime metrics tests
    RUN_TEST(test_runtime_metrics_first_sample_establishes_baseline);
    RUN_TEST(test_runtime_metrics_updates_after_fixed_interval);
    RUN_TEST(test_runtime_metrics_snapshot_is_observational);
    RUN_TEST(test_runtime_metrics_ignores_subinterval_polling);

    // Streamlined serial management tests
    RUN_TEST(test_management_cli_parses_get_status);
    RUN_TEST(test_management_cli_parses_semantic_mode_and_clock_values);
    RUN_TEST(test_management_cli_parses_gpio_path_and_level);
    RUN_TEST(test_management_cli_parses_actions_and_wipe_confirmation);
    RUN_TEST(test_management_cli_rejects_unknown_trailing_and_oversized_lines);
    RUN_TEST(test_management_cli_parses_reset_stats_action);
    RUN_TEST(test_management_cli_formats_framed_success_and_error);
    RUN_TEST(test_management_cli_formats_inverted_signals_for_humans);
    RUN_TEST(test_management_cli_human_commands_keep_original_shortcuts);
    RUN_TEST(test_management_cli_exposes_firmware_version);
    RUN_TEST(test_management_cli_human_commands_accept_original_category_syntax);
    RUN_TEST(test_management_cli_help_is_human_readable);
    RUN_TEST(test_management_cli_help_provides_targeted_usage);

    // HTTP REST management tests
    RUN_TEST(test_management_http_maps_get_and_nested_paths);
    RUN_TEST(test_management_http_exposes_firmware_version);
    RUN_TEST(test_management_http_maps_put_values_through_common_request_semantics);
    RUN_TEST(test_management_http_maps_actions_and_enforces_wipe_confirmation);
    RUN_TEST(test_management_http_rejects_incomplete_oversized_and_unsupported_requests);
    RUN_TEST(test_management_http_maps_reset_stats_action);
    RUN_TEST(test_management_http_formats_bounded_json_success_and_error);
    RUN_TEST(test_management_http_server_opens_socket_two_nonblocking_on_port_80);
    RUN_TEST(test_management_http_server_listens_without_blocking);
    RUN_TEST(test_management_http_server_accumulates_fragments_and_does_one_io_per_poll);
    RUN_TEST(test_management_http_formats_shared_status_and_config_snapshots);
    RUN_TEST(test_management_http_server_sends_incrementally_then_applies_effect);

    // Event Queue Tests
    RUN_TEST(test_event_queue_empty_after_init);
    RUN_TEST(test_event_queue_push_pop_single);
    RUN_TEST(test_event_queue_full_when_capacity_reached);
    RUN_TEST(test_event_queue_pop_empty_fails);
    RUN_TEST(test_event_queue_statistics_reset_preserves_pending_events);
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
    RUN_TEST(test_hdlc_sync_statistics_reset_preserves_accumulator_state);
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

    // TX queue tests
    RUN_TEST(test_tx_queue_init_empty);
    RUN_TEST(test_tx_queue_enqueue_single);
    RUN_TEST(test_tx_queue_full_rejected);
    RUN_TEST(test_tx_queue_oversized_payload_rejected);
    RUN_TEST(test_tx_queue_restarts_midframe_after_cts_edge);
    RUN_TEST(test_tx_queue_protects_queued_tail_until_pio_completion);
    RUN_TEST(test_tx_queue_waits_for_rts_release_between_queued_frames);
    RUN_TEST(test_tx_queue_statistics_reset_preserves_queued_frames);

    // Event scheduler tests
    RUN_TEST(test_event_scheduler_blocks_control_during_active_frame);
    RUN_TEST(test_event_scheduler_services_boundary_with_backlog);
    RUN_TEST(test_event_scheduler_services_full_idle_budget);
    RUN_TEST(test_event_scheduler_blocks_background_io_during_active_frame);
    RUN_TEST(test_event_scheduler_allows_background_io_at_frame_boundary);
    RUN_TEST(test_rx_hunt_filter_drops_only_redundant_preframe_idle);
    RUN_TEST(test_rx_hunt_filter_preserves_repeated_zeroes_after_opening_flag);
    RUN_TEST(test_rx_hunt_filter_inactive_states_never_drop);
    RUN_TEST(test_tx_queue_active_frame_covers_physical_completion);
    RUN_TEST(test_tx_queue_active_frame_clears_before_next_queued_entry);

    // V.24 TX state tests
    RUN_TEST(test_v24_tx_state_asserts_rts_and_waits_for_settle);
    RUN_TEST(test_v24_tx_state_ignores_cts_edges_before_first_byte);
    RUN_TEST(test_v24_tx_state_restarts_on_cts_edge_while_sending);
    RUN_TEST(test_v24_tx_state_restarts_on_cts_edge_while_draining);
    RUN_TEST(test_v24_tx_state_ignores_cts_restart_when_disabled);
    RUN_TEST(test_v24_tx_state_restarts_on_fifo_underrun_during_poll);
    RUN_TEST(test_v24_tx_state_restarts_on_fifo_underrun_before_write);
    RUN_TEST(test_v24_tx_state_completes_after_rts_and_interframe_holds);
    RUN_TEST(test_v24_tx_state_reconfigure_resets_lifecycle_and_preserves_restart);

    // W5500 socket I/O tests
    RUN_TEST(test_w5500_udp_tx_accepts_exact_length_send);
    RUN_TEST(test_w5500_udp_tx_rejects_negative_send);
    RUN_TEST(test_w5500_udp_tx_rejects_short_send);
    RUN_TEST(test_w5500_udp_tx_rejects_invalid_frame);

    // MAC address tests
    RUN_TEST(test_mac_address_from_ipv4_encodes_unique_local_unicast_address);
    RUN_TEST(test_mac_address_from_ipv4_distinguishes_configured_board_ips);

    return UNITY_END();
}
