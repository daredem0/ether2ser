/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/event_loop.c
 * Purpose: Main polling loop driving data path and control-plane dispatch.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "system/event_loop.h"

// Standard library headers
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Library Headers
#include "hardware/watchdog.h"
#include "pico/time.h"
#include "pico/types.h"

// Project Headers
#include "drivers/pio_tx_rx_driver.h"
#include "drivers/tx_queue.h"
#include "drivers/w5500_driver.h"
#include "protocol/hdlc_common.h"
#include "protocol/hdlc_decoder.h"
#include "protocol/hdlc_sync.h"
#include "system/app_context.h"
#include "system/cli_usb_cdc.h"
#include "system/common.h"
#include "system/error.h"
#include "system/event_dispatch.h"
#include "system/event_queue.h"
// Generated headers

#define MAIN_LOOP_SLEEP_MS 1
#define MAIN_LOOP_SLEEP_US 50

#define TX_QUEUE_DRAIN_CHUNK_SIZE 32
#define EVENT_LOOP_MAX_EVENTS_AT_ONCE 20
#define EVENT_LOOP_MAX_EVENTS_WHILE_TX_ACTIVE 2
#define HDLC_DECODE_FAIL_STREAK_LIMIT 4U
#define HDLC_SYNC_IDLE_TIMEOUT_US 20000U
#define HDLC_SYNC_NO_PROGRESS_MAX_BYTES 2048U
#define HDLC_SYNC_NO_PROGRESS_MAX_BYTES_EXTERNAL 12288

static void print_prompt(app_ctx_t* app)
{
    if (app->need_prompt)
    {
        LOG_PLAIN("ether2ser> ");
        app->need_prompt = false;
    }
}

static bool event_loop_should_drop_hunt_idle_byte(app_ctx_t* app, uint8_t rx_byte,
                                                  uint32_t* idle_run_length)
{
    if (!app || !idle_run_length)
    {
        return false;
    }

    bool in_external_hunting =
        app->v24_config.external_clock && (app->accumulator.state == HDLC_SYNC_STATE_HUNTING);
    if (!in_external_hunting)
    {
        *idle_run_length = 0U;
        return false;
    }

    if (rx_byte == 0x00U || rx_byte == 0xFFU)
    {
        if (*idle_run_length >= 2U)
        {
            return true;
        }
        (*idle_run_length)++;
        return false;
    }

    *idle_run_length = 0U;
    return false;
}

typedef struct
{
    uint8_t  rx_byte;
    uint8_t  hdlc_decode_fail_streak;
    uint64_t last_rx_byte_us;
    uint64_t last_frame_ready_bytes;
    uint32_t hunt_idle_run_length;
    bool     work_done;
} event_loop_runtime_t;

static void drain_rx_until_empty(app_ctx_t* app, event_loop_runtime_t* runtime, size_t* rx_drained)
{
    while (rx_get(&runtime->rx_byte))
    {
        runtime->work_done = true;
        (*rx_drained)++;
        if (event_loop_should_drop_hunt_idle_byte(app, runtime->rx_byte,
                                                  &runtime->hunt_idle_run_length))
        {
            app->stats.hunt_idle_drop_bytes++;
            continue;
        }
        if (!hdlc_sync_acc_process_byte(&app->accumulator, runtime->rx_byte))
        {
            app->stats.serial_rx_drop_acc_full++;
        }
    }
}

static void update_rx_drain_stats(app_ctx_t* app, event_loop_runtime_t* event_loop_runtime,
                                  const size_t rx_drained_early, const uint64_t now_us)
{

    app->stats.serial_rx_bytes += rx_drained_early;
    if (app->accumulator.position > app->stats.accumulator_pos_max)
    {
        app->stats.accumulator_pos_max = app->accumulator.position;
    }
    if (rx_drained_early > 0U)
    {
        event_loop_runtime->last_rx_byte_us = now_us;
    }
}

static void poll_and_enqueue_udp_rx(app_ctx_t* app, event_loop_runtime_t* event_loop_runtime)
{

    if (w5500_poll_rx(&app->sender_config, &app->rx_frame_buffer))
    {
        app->stats.udp_rx_frames++;
        e2s_error_t enqueue_result =
            tx_queue_enqueue_udp_frame(&app->tx_queue, &app->rx_frame_buffer);
        if (enqueue_result == E2S_OK)
        {
            event_loop_runtime->work_done = true;
            app->stats.hdlc_tx_frames++;
        }
        else
        {
            if (enqueue_result == E2S_ERR_TX_QUEUE_FULL)
            {
                app->stats.tx_queue_drop_frames++;
            }
            LOG_ERROR("TX Queue Enqueue failed: %d.\r\n", enqueue_result);
        }
        memset(app->rx_frame_buffer.payload, 0, app->rx_frame_buffer.length);
        app->rx_frame_buffer.length = 0;
    }
}

static void poll_tx_pipeline(app_ctx_t* app, event_loop_runtime_t* event_loop_runtime)
{
    if (poll_queue_stats(&app->tx_queue) != E2S_OK)
    {
        LOG_ERROR("Poll Queue Stats failed.\r\n");
    }

    // Poll the tx queue. This writes out bytes on the serial line
    if (tx_queue_drain(&app->tx_queue, TX_QUEUE_DRAIN_CHUNK_SIZE) != E2S_OK)
    {
        LOG_ERROR("Poll Queue Drain failed.\r\n");
    }
    else
    {
        event_loop_runtime->work_done = true;
    }

    // If the tx queue is empty check if the fifo is empty to and reset rts
    if (tx_queue_is_empty(&app->tx_queue))
    {
        // Currently we dont evaluate the result, but the api offers it
        (void)tx_poll();
    }
}

static void poll_hdlc_idle_timeout(app_ctx_t* app, event_loop_runtime_t* event_loop_runtime,
                                   const uint64_t now_us)
{
    bool frame_in_progress = (app->accumulator.state != HDLC_SYNC_STATE_HUNTING) ||
                             (app->accumulator.position > 0U) ||
                             (app->reconstructed_frame.length > 0U);
    if (frame_in_progress && event_loop_runtime->last_rx_byte_us != 0U &&
        (now_us - event_loop_runtime->last_rx_byte_us) > HDLC_SYNC_IDLE_TIMEOUT_US)
    {
        app->stats.resync_idle_timeout_count++;
        LOG_DEBUG("HDLC idle timeout resync\r\n");
        hdlc_sync_acc_init(&app->accumulator, HDLC_FLAG_BYTE);
        event_loop_runtime->hdlc_decode_fail_streak = 0U;
        memset(app->reconstructed_frame.payload, 0, app->reconstructed_frame.length);
        app->reconstructed_frame.length = 0U;
    }
}

static void decode_hdlc_to_udp_tx(app_ctx_t* app, event_loop_runtime_t* event_loop_runtime)
{
    event_loop_runtime->work_done = true;
    app->stats.hdlc_frame_ready++;
    event_loop_runtime->last_frame_ready_bytes = app->stats.serial_rx_bytes;
    app->tx_frame_buffer.length                = 0;
    if (hdlc_decode(&app->reconstructed_frame, app->tx_frame_buffer.payload, TX_BUF_SIZE,
                    &app->tx_frame_buffer.length, true))
    {
        app->stats.hdlc_decode_ok++;
        event_loop_runtime->hdlc_decode_fail_streak = 0;
        w5500_udp_tx(&app->destination_config, &app->tx_frame_buffer);
        app->stats.udp_tx_frames++;
        hdlc_sync_acc_consume_candidate(&app->accumulator, true);
    }
    else
    {
        app->stats.hdlc_decode_fail++;
        if (app->stats.hdlc_decode_fail == 1U)
        {
            LOG_DEBUG("HDLC first decode fail: off=%u shift_right=%u cand_start=%zu "
                      "cand_end=%zu pos=%zu proc=%zu\r\n",
                      (unsigned)app->accumulator.bit_offset,
                      (unsigned)app->accumulator.align_shift_right,
                      app->accumulator.candidate_start, app->accumulator.candidate_end,
                      app->accumulator.position, app->accumulator.processed);
        }
        hdlc_sync_acc_consume_candidate(&app->accumulator, false);
        if (event_loop_runtime->hdlc_decode_fail_streak < UINT8_MAX)
        {
            event_loop_runtime->hdlc_decode_fail_streak++;
        }
        if (event_loop_runtime->hdlc_decode_fail_streak >= HDLC_DECODE_FAIL_STREAK_LIMIT)
        {
            app->stats.resync_hard_fail_count++;
            LOG_DEBUG("HDLC hard resync after %u decode fails\r\n",
                      (unsigned)event_loop_runtime->hdlc_decode_fail_streak);
            hdlc_sync_acc_init(&app->accumulator, HDLC_FLAG_BYTE);
            event_loop_runtime->hdlc_decode_fail_streak = 0;
        }
    }
    memset(app->reconstructed_frame.payload, 0, app->reconstructed_frame.length);
    app->reconstructed_frame.length = 0;
}

static void drain_hdlc_frames_to_udp(app_ctx_t* app, event_loop_runtime_t* event_loop_runtime)
{
    while (true)
    {
        e2s_error_t acc_result = hdlc_sync_acc_poll(&app->accumulator, &app->reconstructed_frame);
        if (acc_result == E2S_ERR_HDLC_ACC_FRAME_READY)
        {
            decode_hdlc_to_udp_tx(app, event_loop_runtime);
            continue;
        }
        if (acc_result != E2S_OK)
        {
            app->reconstructed_frame.length = 0;
            hdlc_sync_acc_init(&app->accumulator, HDLC_FLAG_BYTE);
        }
        break;
    }
}
static void poll_hdlc_no_progress(app_ctx_t* app, event_loop_runtime_t* event_loop_runtime)
{
    // External-clock mode continuously feeds idle bytes. In HUNTING this is normal and
    // must not trigger no-progress resyncs, otherwise the next real frame gets clipped.
    bool decode_in_progress = (app->accumulator.state != HDLC_SYNC_STATE_HUNTING) ||
                              app->accumulator.candidate_valid ||
                              (app->reconstructed_frame.length > 0U);
    if (!decode_in_progress)
    {
        // In external-clock idle periods serial_rx_bytes keeps increasing.
        // Reset baseline while idle so first frame after a gap is not
        // immediately forced into no-progress resync.
        event_loop_runtime->last_frame_ready_bytes = app->stats.serial_rx_bytes;
    }
    bool           have_decode_lock      = (app->stats.hdlc_decode_ok > 0U);
    const uint64_t no_progress_max_bytes = app->v24_config.external_clock
                                               ? HDLC_SYNC_NO_PROGRESS_MAX_BYTES_EXTERNAL
                                               : HDLC_SYNC_NO_PROGRESS_MAX_BYTES;
    if (have_decode_lock && decode_in_progress &&
        (app->stats.serial_rx_bytes > event_loop_runtime->last_frame_ready_bytes) &&
        ((app->stats.serial_rx_bytes - event_loop_runtime->last_frame_ready_bytes) >
         no_progress_max_bytes))
    {
        app->stats.resync_no_progress_count++;
        LOG_DEBUG("HDLC no-progress resync after %" PRIu64 " bytes (limit=%" PRIu64 ")\r\n",
                  (app->stats.serial_rx_bytes - event_loop_runtime->last_frame_ready_bytes),
                  no_progress_max_bytes);
        hdlc_sync_acc_init(&app->accumulator, HDLC_FLAG_BYTE);
        event_loop_runtime->hdlc_decode_fail_streak = 0U;
        memset(app->reconstructed_frame.payload, 0, app->reconstructed_frame.length);
        app->reconstructed_frame.length            = 0U;
        event_loop_runtime->last_frame_ready_bytes = app->stats.serial_rx_bytes;

        rx_clock_hard_reset();
    }
}

static void update_statistics(app_ctx_t* app)
{
    app->stats.sync_lookahead_wait_syncing = app->accumulator.lookahead_wait_syncing;
    app->stats.sync_lookahead_wait_synced  = app->accumulator.lookahead_wait_synced;
    app->stats.sync_candidate_consume      = app->accumulator.consume_count;
    app->stats.sync_hardcap_drop_events    = app->accumulator.hardcap_drop_events;
    app->stats.sync_hardcap_drop_bytes     = app->accumulator.hardcap_drop_bytes;

    hdlc_decode_stats_t decode_stats = {0};
    hdlc_decode_stats_snapshot(&decode_stats);
    app->stats.decode_fail_invalid_frame    = decode_stats.invalid_frame;
    app->stats.decode_fail_too_short        = decode_stats.too_short;
    app->stats.decode_fail_payload_too_long = decode_stats.payload_too_long;
    app->stats.decode_fail_unstuff_error    = decode_stats.unstuff_error;
    app->stats.decode_fail_crc_mismatch     = decode_stats.crc_mismatch;

    size_t tx_queue_count = tx_queue_get_count(&app->tx_queue);
    if (tx_queue_count > app->stats.tx_queue_used_max)
    {
        app->stats.tx_queue_used_max = tx_queue_count;
    }
    size_t event_queue_count = event_queue_get_count();
    if (event_queue_count > app->stats.event_queue_used_max)
    {
        app->stats.event_queue_used_max = event_queue_count;
    }
    size_t event_queue_hwm = event_queue_get_high_water_mark();
    if (event_queue_hwm > app->stats.event_queue_used_max)
    {
        app->stats.event_queue_used_max = event_queue_hwm;
    }
    app->stats.event_queue_drop_events = event_queue_get_push_drop_count();
    app->stats.log_drop_lines += log_take_dropped_count();
    app->stats.log_queue_used_max = log_get_high_water_mark();
}

static void poll_and_dispatch_events(app_ctx_t* app)
{
    bool tx_active = !tx_queue_is_empty(&app->tx_queue);
    int  max_events_per_iteration =
        tx_active ? EVENT_LOOP_MAX_EVENTS_WHILE_TX_ACTIVE : EVENT_LOOP_MAX_EVENTS_AT_ONCE;

    event_t event_item;
    for (int i = 0; i < max_events_per_iteration && event_queue_pop(&event_item); i++)
    {
        event_dispatch(&event_item, app);
    }
}

void event_loop(app_ctx_t* app)
{
    static event_loop_runtime_t event_loop_runtime = {.rx_byte                 = 0U,
                                                      .hdlc_decode_fail_streak = 0U,
                                                      .last_rx_byte_us         = 0U,
                                                      .last_frame_ready_bytes  = 0U,
                                                      .hunt_idle_run_length    = 0U,
                                                      .work_done               = false};

    while (true)
    {
        watchdog_update();

        // Poll the event queue
        cli_poll();

        // Drain RX early in the loop to reduce risk of RX FIFO stalling under continuous clock.
        size_t rx_drained_early = 0;
        drain_rx_until_empty(app, &event_loop_runtime, &rx_drained_early);
        update_rx_drain_stats(app, &event_loop_runtime, rx_drained_early,
                              to_us_since_boot(get_absolute_time()));
        poll_and_enqueue_udp_rx(app, &event_loop_runtime);

        poll_tx_pipeline(app, &event_loop_runtime);

        // Drain RX FIFO into the accumulator buffer
        size_t rx_drained = 0;
        drain_rx_until_empty(app, &event_loop_runtime, &rx_drained);
        uint64_t now_us = to_us_since_boot(get_absolute_time());
        update_rx_drain_stats(app, &event_loop_runtime, rx_drained, now_us);

        /** TODO: Its not good that the event loop pokes around in hdlc state.
         *  We should add a hhelper like hdlc_sync_acc_reset_frame that hides
         * these details and just call that from here. This shall be refactored
         * in the next iteration.
         */
        poll_hdlc_idle_timeout(app, &event_loop_runtime, now_us);

        // Try to extract all complete HDLC frames currently buffered.
        drain_hdlc_frames_to_udp(app, &event_loop_runtime);

        poll_hdlc_no_progress(app, &event_loop_runtime);

        update_statistics(app);
        if (rx_clock_poll_stall())
        {
            (app->stats.rx_fifo_stall_events)++;
        }

        poll_and_dispatch_events(app);
        if (log_take_emitted_flag())
        {
            app->need_prompt = true;
        }
        print_prompt(app);

        if (!event_loop_runtime.work_done)
        {
            sleep_us(MAIN_LOOP_SLEEP_US);
        }
        else
        {
            event_loop_runtime.work_done = false;
        }
    }
}
