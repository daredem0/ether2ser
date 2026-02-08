
// Related headers
#include "system/event_loop.h"

// Standard library headers
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

// Library Headers
#include "pico/stdio.h"
#include "pico/time.h"
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"

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

static void print_prompt(app_ctx_t* app)
{
    if (app->need_prompt)
    {
        printf("ether2ser> ");
        app->need_prompt = false;
    }
}

void event_loop(app_ctx_t* app)
{
    static uint8_t rx_byte = 0;
    while (true)
    {
        // Poll the event queue
        cli_poll();
        if (w5500_poll_rx(&app->sender_config, &app->rx_frame_buffer))
        {
            app->stats.udp_rx_frames++;
            e2s_error_t enqueue_result =
                tx_queue_enqueue_udp_frame(&app->tx_queue, &app->rx_frame_buffer);
            if (enqueue_result == E2S_OK)
            {
                app->stats.hdlc_tx_frames++;
            }
            memset(app->rx_frame_buffer.payload, 0, app->rx_frame_buffer.length);
            app->rx_frame_buffer.length = 0;
        }
        poll_queue_stats(&app->tx_queue);

        // Poll the tx queue. This writes out bytes on the serial line
        tx_queue_drain(&app->tx_queue, 32);

        // If the tx queue is empty check if the fifo is empty to and reset rts
        if (tx_queue_is_empty(&app->tx_queue))
        {
            tx_poll();
        }

        // Drain RX FIFO into the accumulator buffer
        size_t rx_drained = 0;
        while (rx_get(&rx_byte))
        {
            rx_drained++;
            hdlc_sync_acc_process_byte(&app->accumulator, rx_byte);
        }
        app->stats.serial_rx_bytes += rx_drained;

        // Try to extract all complete HDLC frames currently buffered.
        while (true)
        {
            e2s_error_t acc_result =
                hdlc_sync_acc_poll(&app->accumulator, &app->reconstructed_frame);
            if (acc_result == E2S_ERR_HDLC_ACC_FRAME_READY)
            {
                app->stats.hdlc_frame_ready++;
                app->tx_frame_buffer.length = 0;
                if (hdlc_decode(&app->reconstructed_frame, app->tx_frame_buffer.payload,
                                TX_BUF_SIZE, &app->tx_frame_buffer.length, true))
                {
                    app->stats.hdlc_decode_ok++;
                    w5500_udp_tx(&app->destination_config, &app->tx_frame_buffer);
                    app->stats.udp_tx_frames++;
                    hdlc_sync_acc_consume_candidate(&app->accumulator, true);
                }
                else
                {
                    app->stats.hdlc_decode_fail++;
                    hdlc_sync_acc_consume_candidate(&app->accumulator, false);
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

        app->stats.sync_lookahead_wait_syncing = app->accumulator.lookahead_wait_syncing;
        app->stats.sync_lookahead_wait_synced  = app->accumulator.lookahead_wait_synced;
        app->stats.sync_candidate_consume      = app->accumulator.consume_count;
        app->stats.sync_hardcap_drop_events    = app->accumulator.hardcap_drop_events;
        app->stats.sync_hardcap_drop_bytes     = app->accumulator.hardcap_drop_bytes;

        event_t event_item;
        for (int i = 0; i < 20 && event_queue_pop(&event_item); i++)
        {
            event_dispatch(&event_item, app);
        }
        if (log_take_emitted_flag())
        {
            app->need_prompt = true;
        }
        print_prompt(app);
        sleep_us(MAIN_LOOP_SLEEP_US);
    }
}
