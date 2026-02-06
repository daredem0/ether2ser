
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
        w5500_poll_rx(&app->sender_config, &app->rx_frame_buffer);
        poll_queue_stats(&app->tx_queue);

        // Poll the tx queue. This writes out bytes on the serial line
        tx_queue_drain(&app->tx_queue, 4);

        // If the tx queue is empty check if the fifo is empty to and reset rts
        if (tx_queue_is_empty(&app->tx_queue))
        {
            tx_poll();
        }

        // Try to read a byte and push it into the accumulator buffer
        if (rx_get(&rx_byte))
        {
            hdlc_sync_acc_process_byte(&app->accumulator, rx_byte);
        }

        // Try to get a full hdlc frame
        if (hdlc_sync_acc_poll(&app->accumulator, &app->reconstructed_frame) ==
            E2S_ERR_HDLC_ACC_FRAME_READY)
        {
            event_t hdlc_frame_event = {.type      = EV_HDLC_DECODE,
                                        .data.ptr  = &app->reconstructed_frame,
                                        .data_len  = sizeof(app->reconstructed_frame),
                                        .is_inline = false};
            event_queue_push(&hdlc_frame_event);
        }
        else
        {
            // Reset state if end wasnt found. We need to hunt again
            app->accumulator.state          = HDLC_SYNC_STATE_HUNTING;
            app->reconstructed_frame.length = 0;
        }

        event_t event_item;
        for (int i = 0; i < 2 && event_queue_pop(&event_item); i++)
        {
            event_dispatch(&event_item, app);
        }
        print_prompt(app);
        sleep_us(MAIN_LOOP_SLEEP_US);
    }
}
