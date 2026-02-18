
/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/app_context.h
 * Purpose: Central application context and shared runtime state types.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef APP_CONTEXT_H
#define APP_CONTEXT_H

// Related headers

// Standard library headers
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Library Headers
#include "pico/stdio.h"
#include "pico/time.h"

// Project Headers
#include "drivers/pio_tx_rx_driver.h"
#include "drivers/tx_queue.h"
#include "drivers/w5500_driver.h"
#include "platform/pinmap.h"
#include "protocol/hdlc_common.h"
#include "protocol/hdlc_decoder.h"
#include "protocol/hdlc_sync.h"
#include "system/baudrate_monitor.h"
#include "system/cli_commands.h"
#include "system/cli_usb_cdc.h"
#include "system/common.h"
#include "system/event_queue.h"
#include "system/persistent_config.h"

// Generated headers

/**
 * @brief Runtime pipeline statistics and counters.
 */
typedef struct
{
    uint64_t udp_rx_frames;
    uint64_t hdlc_tx_frames;
    uint64_t serial_rx_bytes;
    uint64_t hdlc_frame_ready;
    uint64_t hdlc_decode_ok;
    uint64_t hdlc_decode_fail;
    uint64_t udp_tx_frames;
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

    uint32_t last_report_ms;
} payload_statistics_t;

/**
 * @brief Global application context shared across modules.
 */
typedef struct
{
    config_t persistent_config;
    bool     config_valid;
    bool     need_prompt;

    UDP_CONFIG_T     local_config;
    UDP_CONFIG_T     destination_config;
    UDP_CONFIG_T     sender_config;
    NETWORK_CONFIG_T net_config;

    V24_CONFIG_T v24_config;

    uint8_t     rx_frame_buffer_data[RX_BUF_SIZE];
    UDP_FRAME_T rx_frame_buffer;
    uint8_t     tx_frame_buffer_data[TX_BUF_SIZE];
    UDP_FRAME_T tx_frame_buffer;

    uint8_t                 reconstructed_frame_buffer[RX_HDLC_SYNC_MAX_BUFFER_SIZE];
    HDLC_FRAME_T            reconstructed_frame;
    HDLC_SYNC_ACCUMULATOR_T accumulator;

    uint8_t              tx_queue_buffer[TX_FRAME_QUEUE_SIZE * sizeof(TX_QUEUE_ENTRY_T)];
    TX_QUEUE_T           tx_queue;
    payload_statistics_t stats;
    // ... anything else the event loop touches
} app_ctx_t;

/**
 * @brief Initialize application context from persistent/default configuration.
 * @param app Application context to initialize.
 * @param persistent_config Source configuration (used when marked valid).
 */
void init_app(app_ctx_t* app, const config_t* persistent_config);

/**
 * @brief Get access to the app context. This context is owned by main, app_init
 * just takes hold of a pointer to it.
 * @return Pointer to the app context.
 */
app_ctx_t* get_app_ctx(void);

#endif /* APP_CONTEXT_H */
