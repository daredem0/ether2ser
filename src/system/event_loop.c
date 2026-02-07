
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
        LOG_DEBUG("ether2ser> ");
        app->need_prompt = false;
    }
}

void event_loop(app_ctx_t* app)
{
    static uint8_t  rx_byte                = 0;
    static uint32_t udp_rx_count           = 0;
    static uint32_t hdlc_tx_count          = 0;
    static uint32_t hdlc_rx_count          = 0;
    static uint32_t udp_tx_count           = 0;
    static uint32_t hdlc_decode_fail_count = 0;
    static uint32_t hdlc_decode_ok_count   = 0;
    while (true)
    {
        // Poll the event queue
        cli_poll();
        if (w5500_poll_rx(&app->sender_config, &app->rx_frame_buffer))
        {
            udp_rx_count++;
            LOG_DEBUG("UDP RX COUNT: %lu\n", (unsigned long)udp_rx_count);
            LOG_DEBUG("tx_queue_enqueue_udp_frame: %d\r\n",
                      tx_queue_enqueue_udp_frame(&app->tx_queue, &app->rx_frame_buffer));
            hdlc_tx_count++;
            LOG_DEBUG("HDLC TX COUNT: %lu\n", (unsigned long)hdlc_tx_count);
            memset(app->rx_frame_buffer.payload, 0, app->rx_frame_buffer.length);
            app->rx_frame_buffer.length = 0;
            app->need_prompt            = true;
        }
        poll_queue_stats(&app->tx_queue);

        // Poll the tx queue. This writes out bytes on the serial line
        tx_queue_drain(&app->tx_queue, 32);

        // If the tx queue is empty check if the fifo is empty to and reset rts
        if (tx_queue_is_empty(&app->tx_queue))
        {
            tx_poll();
        }
        // Add instrumentation to detect repeated PIO stalls
        // static uint32_t pio_stall_count = 0;
        // if (pio0->fdebug & (1u << (PIO_FDEBUG_TXSTALL_LSB + 0)))
        // {
        //     pio_stall_count++;
        //     if (pio_stall_count % 1000 == 0)
        //     {
        //         LOG_DEBUG("PIO TX STALLED: %lu times\r\n", pio_stall_count);
        //     }
        // }

        // Drain RX FIFO into the accumulator buffer
        static uint32_t total_rx_bytes = 0;
        while (rx_get(&rx_byte))
        {
            total_rx_bytes++;
            hdlc_sync_acc_process_byte(&app->accumulator, rx_byte);
        }

        static bool   first_frame_logged = false;
        static size_t max_position       = 0;
        if (app->accumulator.position > max_position)
        {
            max_position = app->accumulator.position;
        }
        if (!first_frame_logged && udp_rx_count >= 1 && hdlc_rx_count >= 0)
        {
            printf("After first frame: accumulator max position was %zu bytes\n", max_position);
            first_frame_logged = true;
        }
        static bool     rx_count_logged = false;
        static uint32_t last_log_time   = 0;
        uint32_t        now             = to_ms_since_boot(get_absolute_time());
        if (!rx_count_logged && now > 5000)
        {
            printf("Total RX bytes received: %lu\n", (unsigned long)total_rx_bytes);
            rx_count_logged = true;
        }

        // Try to extract all complete HDLC frames currently buffered.
        while (true)
        {
            e2s_error_t acc_result =
                hdlc_sync_acc_poll(&app->accumulator, &app->reconstructed_frame);
            if (acc_result == E2S_ERR_HDLC_ACC_FRAME_READY)
            {
                hdlc_rx_count++;
                LOG_DEBUG("HDLC RX COUNT: %lu\n", (unsigned long)hdlc_rx_count);
                app->tx_frame_buffer.length = 0;
                if (hdlc_decode(&app->reconstructed_frame, app->tx_frame_buffer.payload,
                                TX_BUF_SIZE, &app->tx_frame_buffer.length, true))
                {
                    hdlc_decode_ok_count++;
                    LOG_DEBUG("HDLC DECODE OK: %lu\n", (unsigned long)hdlc_decode_ok_count);
                    w5500_udp_tx(&app->destination_config, &app->tx_frame_buffer);
                    udp_tx_count++;
                    LOG_DEBUG("UDP TX COUNT: %lu\n", (unsigned long)udp_tx_count);
                    hdlc_sync_acc_consume_candidate(&app->accumulator, true);
                }
                else
                {
                    hdlc_decode_fail_count++;
                    LOG_DEBUG("HDLC DECODE FAIL: %lu\n", (unsigned long)hdlc_decode_fail_count);
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

        event_t event_item;
        for (int i = 0; i < 20 && event_queue_pop(&event_item); i++)
        {
            event_dispatch(&event_item, app);
        }
        print_prompt(app);
        sleep_us(MAIN_LOOP_SLEEP_US);
    }
}
