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
            hdlc_sync_acc_process_byte(&app->accumulator, rx_byte);
        }
        app->stats.serial_rx_bytes += rx_drained;
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
                    hdlc_sync_acc_consume_candidate(&app->accumulator, false);
                    if (hdlc_decode_fail_streak < UINT8_MAX)
                    {
                        hdlc_decode_fail_streak++;
                    }
                    if (hdlc_decode_fail_streak >= HDLC_DECODE_FAIL_STREAK_LIMIT)
                    {
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
        if ((app->stats.serial_rx_bytes > last_frame_ready_bytes) &&
            ((app->stats.serial_rx_bytes - last_frame_ready_bytes) >
             HDLC_SYNC_NO_PROGRESS_MAX_BYTES))
        {
            LOG_DEBUG("HDLC no-progress resync after %" PRIu64 " bytes\r\n",
                      (app->stats.serial_rx_bytes - last_frame_ready_bytes));
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
