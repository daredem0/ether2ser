#ifndef RUNTIME_STATS_H
#define RUNTIME_STATS_H

#include <stdint.h>

typedef struct
{
    uint64_t udp_rx_frames;
    uint64_t hdlc_tx_frames;
    uint64_t serial_rx_bytes;
    uint64_t hdlc_frame_ready;
    uint64_t hdlc_decode_ok;
    uint64_t hdlc_decode_fail;
    uint64_t udp_tx_attempt_frames;
    uint64_t udp_tx_success_frames;
    uint64_t udp_tx_failed_frames;
    uint64_t udp_rx_throttle_enter;
    uint64_t udp_rx_throttle_skips;
    uint64_t udp_rx_buffer_full_counts;
    uint64_t udp_tx_buffer_full_counts;
    uint64_t sync_lookahead_wait_syncing;
    uint64_t sync_lookahead_wait_synced;
    uint64_t sync_candidate_consume;
    uint64_t sync_hardcap_drop_events;
    uint64_t sync_hardcap_drop_bytes;
    uint64_t decode_fail_invalid_frame;
    uint64_t decode_fail_too_short;
    uint64_t decode_fail_payload_too_long;
    uint64_t decode_fail_unstuff_error;
    uint64_t decode_fail_crc_mismatch;
    uint64_t resync_idle_timeout_count;
    uint64_t resync_hard_fail_count;
    uint64_t resync_no_progress_count;
    uint64_t tx_queue_used_max;
    uint64_t tx_queue_drop_frames;
    uint64_t event_queue_used_max;
    uint64_t event_queue_drop_events;
    uint64_t log_drop_lines;
    uint64_t log_queue_used_max;
    uint64_t accumulator_pos_max;
    uint64_t rx_fifo_stall_events;
    uint64_t serial_rx_drop_acc_full;
    uint64_t hunt_idle_drop_bytes;
} payload_statistics_t;

#endif /* RUNTIME_STATS_H */
