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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Library Headers
#include "hardware/regs/pio.h"
#include "hardware/watchdog.h"
#include "pico/time.h"

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

void event_loop(app_ctx_t* app)
{
    static uint8_t  rx_byte                 = 0;
    static uint8_t  hdlc_decode_fail_streak = 0;
    static uint64_t last_rx_byte_us         = 0U;
    static uint64_t last_frame_ready_bytes  = 0U;
    bool            work_done               = false;
    while (true)
    {
        watchdog_update();
        // Poll the event queue
        cli_poll();

        // Drain RX early in the loop to reduce risk of RX FIFO stalling under continuous clock.
        size_t rx_drained_early = 0;
        while (rx_get(&rx_byte))
        {
            work_done = true;
            rx_drained_early++;
            if (!hdlc_sync_acc_process_byte(&app->accumulator, rx_byte))
            {
                app->stats.serial_rx_drop_acc_full++;
            }
        }
        app->stats.serial_rx_bytes += rx_drained_early;
        if (app->accumulator.position > app->stats.accumulator_pos_max)
        {
            app->stats.accumulator_pos_max = app->accumulator.position;
        }
        if (rx_drained_early > 0U)
        {
            last_rx_byte_us = to_us_since_boot(get_absolute_time());
        }

        if (w5500_poll_rx(&app->sender_config, &app->rx_frame_buffer))
        {
            app->stats.udp_rx_frames++;
            e2s_error_t enqueue_result =
                tx_queue_enqueue_udp_frame(&app->tx_queue, &app->rx_frame_buffer);
            if (enqueue_result == E2S_OK)
            {
                work_done = true;
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
            work_done = true;
        }

        // If the tx queue is empty check if the fifo is empty to and reset rts
        if (tx_queue_is_empty(&app->tx_queue))
        {
            // Currently we dont evaluate the result, but the api offers it
            (void)tx_poll();
        }

        // Drain RX FIFO into the accumulator buffer
        size_t rx_drained = 0;
        while (rx_get(&rx_byte))
        {
            work_done = true;
            rx_drained++;
            if (!hdlc_sync_acc_process_byte(&app->accumulator, rx_byte))
            {
                app->stats.serial_rx_drop_acc_full++;
            }
        }
        app->stats.serial_rx_bytes += rx_drained;
        if (app->accumulator.position > app->stats.accumulator_pos_max)
        {
            app->stats.accumulator_pos_max = app->accumulator.position;
        }
        uint64_t now_us = to_us_since_boot(get_absolute_time());
        if (rx_drained > 0U)
        {
            last_rx_byte_us = now_us;
            work_done       = true;
        }

        bool frame_in_progress = (app->accumulator.state != HDLC_SYNC_STATE_HUNTING) ||
                                 (app->accumulator.position > 0U) ||
                                 (app->reconstructed_frame.length > 0U);
        if (frame_in_progress && last_rx_byte_us != 0U &&
            (now_us - last_rx_byte_us) > HDLC_SYNC_IDLE_TIMEOUT_US)
        {
            app->stats.resync_idle_timeout_count++;
            LOG_DEBUG("HDLC idle timeout resync\r\n");
            hdlc_sync_acc_init(&app->accumulator, HDLC_FLAG_BYTE);
            hdlc_decode_fail_streak = 0U;
            memset(app->reconstructed_frame.payload, 0, app->reconstructed_frame.length);
            app->reconstructed_frame.length = 0U;
        }

        // Try to extract all complete HDLC frames currently buffered.
        while (true)
        {
            e2s_error_t acc_result =
                hdlc_sync_acc_poll(&app->accumulator, &app->reconstructed_frame);
            if (acc_result == E2S_ERR_HDLC_ACC_FRAME_READY)
            {
                work_done = true;
                app->stats.hdlc_frame_ready++;
                last_frame_ready_bytes      = app->stats.serial_rx_bytes;
                app->tx_frame_buffer.length = 0;
                if (hdlc_decode(&app->reconstructed_frame, app->tx_frame_buffer.payload,
                                TX_BUF_SIZE, &app->tx_frame_buffer.length, true))
                {
                    app->stats.hdlc_decode_ok++;
                    hdlc_decode_fail_streak = 0;
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
                                  app->accumulator.candidate_start,
                                  app->accumulator.candidate_end, app->accumulator.position,
                                  app->accumulator.processed);
                    }
                    hdlc_sync_acc_consume_candidate(&app->accumulator, false);
                    if (hdlc_decode_fail_streak < UINT8_MAX)
                    {
                        hdlc_decode_fail_streak++;
                    }
                    if (hdlc_decode_fail_streak >= HDLC_DECODE_FAIL_STREAK_LIMIT)
                    {
                        app->stats.resync_hard_fail_count++;
                        LOG_DEBUG("HDLC hard resync after %u decode fails\r\n",
                                  (unsigned)hdlc_decode_fail_streak);
                        hdlc_sync_acc_init(&app->accumulator, HDLC_FLAG_BYTE);
                        hdlc_decode_fail_streak = 0;
                    }
                }
                memset(app->reconstructed_frame.payload, 0, app->reconstructed_frame.length);
                app->reconstructed_frame.length = 0;
                continue;
            }
            if (acc_result != E2S_OK)
            {
                app->reconstructed_frame.length = 0;
                hdlc_sync_acc_init(&app->accumulator, HDLC_FLAG_BYTE);
            }
            break;
        }
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
            last_frame_ready_bytes = app->stats.serial_rx_bytes;
        }
        bool have_decode_lock = (app->stats.hdlc_decode_ok > 0U);
        const uint64_t no_progress_max_bytes = app->v24_config.external_clock
                                                   ? HDLC_SYNC_NO_PROGRESS_MAX_BYTES_EXTERNAL
                                                   : HDLC_SYNC_NO_PROGRESS_MAX_BYTES;
        if (have_decode_lock && decode_in_progress &&
            (app->stats.serial_rx_bytes > last_frame_ready_bytes) &&
            ((app->stats.serial_rx_bytes - last_frame_ready_bytes) > no_progress_max_bytes))
        {
            app->stats.resync_no_progress_count++;
            LOG_DEBUG("HDLC no-progress resync after %" PRIu64 " bytes (limit=%" PRIu64 ")\r\n",
                      (app->stats.serial_rx_bytes - last_frame_ready_bytes), no_progress_max_bytes);
            hdlc_sync_acc_init(&app->accumulator, HDLC_FLAG_BYTE);
            hdlc_decode_fail_streak = 0U;
            memset(app->reconstructed_frame.payload, 0, app->reconstructed_frame.length);
            app->reconstructed_frame.length = 0U;
            last_frame_ready_bytes          = app->stats.serial_rx_bytes;

            const v24_runtime_t* v24_runtime = get_v24_runtime();
            if (v24_runtime->rx_pio != NULL)
            {
                pio_sm_set_enabled(v24_runtime->rx_pio, v24_runtime->rx_sm, false);
                pio_sm_clear_fifos(v24_runtime->rx_pio, v24_runtime->rx_sm);
                pio_sm_restart(v24_runtime->rx_pio, v24_runtime->rx_sm);
                pio_sm_clkdiv_restart(v24_runtime->rx_pio, v24_runtime->rx_sm);
                pio_sm_set_enabled(v24_runtime->rx_pio, v24_runtime->rx_sm, true);
            }
        }

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

        const v24_runtime_t* v24_runtime = get_v24_runtime();
        if (v24_runtime->rx_pio != NULL)
        {
            uint32_t rx_stall_mask = (1U << (PIO_FDEBUG_RXSTALL_LSB + v24_runtime->rx_sm));
            if ((v24_runtime->rx_pio->fdebug & rx_stall_mask) != 0U)
            {
                app->stats.rx_fifo_stall_events++;
                v24_runtime->rx_pio->fdebug = rx_stall_mask;
            }
        }

        bool tx_active = !tx_queue_is_empty(&app->tx_queue);
        int  max_events_per_iteration =
            tx_active ? EVENT_LOOP_MAX_EVENTS_WHILE_TX_ACTIVE : EVENT_LOOP_MAX_EVENTS_AT_ONCE;

        event_t event_item;
        for (int i = 0; i < max_events_per_iteration && event_queue_pop(&event_item); i++)
        {
            event_dispatch(&event_item, app);
        }
        if (log_take_emitted_flag())
        {
            app->need_prompt = true;
        }
        print_prompt(app);

        if (!work_done)
        {
            sleep_us(MAIN_LOOP_SLEEP_US);
        }
    }
}
