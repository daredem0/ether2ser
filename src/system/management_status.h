#ifndef MANAGEMENT_STATUS_H
#define MANAGEMENT_STATUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "protocol/hdlc_decoder.h"
#include "system/runtime_metrics.h"
#include "system/runtime_stats.h"

typedef struct
{
    uint32_t ram_static_used;
    uint32_t heap_allocated;
    uint32_t ram_total;
    uint32_t ram_free;
    uint32_t flash_used;
    uint32_t flash_total;
    uint32_t flash_free;
} management_memory_snapshot_t;

typedef struct
{
    payload_statistics_t stats;
    runtime_metric_rates_t rates;
    hdlc_decode_failure_diagnostic_t first_decode_failure;
    double   rck_hz;
    double   xck_hz;
    bool     xck_available;
    uint64_t tx_ready_gap;
    uint64_t ready_udp_gap;
    uint64_t tx_wire_bytes;
    int      sync_state;
    size_t   accumulator_position;
    size_t   accumulator_processed;
    uint8_t  accumulator_bit_offset;
    bool     accumulator_candidate_valid;
    size_t   accumulator_candidate_end;
    size_t   reconstructed_length;
    size_t   tx_queue_used;
    size_t   tx_queue_capacity;
    bool     tx_frame_active;
    uint32_t dma_overflow_words;
    bool     pio_stalled;
    uint32_t tx_fifo_underruns;
    uint32_t tx_cts_restarts;
    bool     cts_level;
    uint32_t cts_edge_count;
    bool     cts_restart_enabled;
} management_status_snapshot_t;

#endif /* MANAGEMENT_STATUS_H */
