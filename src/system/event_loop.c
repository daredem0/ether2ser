
// Related headers
#include "system/event_loop.h"

// Standard library headers
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Library Headers
#include "pico/stdio.h"
#include "pico/time.h"
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"

// Project Headers
#include "drivers/pio_tx_rx_driver.h"
#include "drivers/tx_queue.h"
#include "drivers/w5500_driver.h"
#include "platform/pinmap.h"
#include "protocol/hdlc_common.h"
#include "protocol/hdlc_decoder.h"
#include "protocol/hdlc_sync.h"
#include "system/app_context.h"
#include "system/baudrate_monitor.h"
#include "system/cli_commands.h"
#include "system/cli_usb_cdc.h"
#include "system/common.h"
#include "system/event_queue.h"
#include "system/persistent_config.h"

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

static void print_v24_polarities(const V24_POLARITIES_T* p)
{
    if (p == NULL)
    {
        printf("polarities: <null>\r\n");
        return;
    }

    printf("polarities: ");
    bool first = true;
#define ADD(name, flag)                             \
    do                                              \
    {                                               \
        if (flag)                                   \
        {                                           \
            printf("%s%s", first ? "" : ",", name); \
            first = false;                          \
        }                                           \
    } while (0)

    ADD("txd", p->tx_polarities.txd_inverted);
    ADD("txc", p->tx_polarities.txc_inverted);
    ADD("cts", p->tx_polarities.cts_inverted);
    ADD("rts", p->tx_polarities.rts_inverted);
    ADD("dtr", p->tx_polarities.dtr_inverted);
    ADD("rxd", p->rx_polarities.rxd_inverted);
    ADD("rxc", p->rx_polarities.rxc_inverted);
    ADD("dcd", p->rx_polarities.dcd_inverted);

    if (first)
    {
        printf("<none>");
    }
    printf("\r\n");
}

static const char* net_setting_id_name(event_queue_data_types_t id)
{
    switch (id)
    {
    case NET_IP_REMOTE:
        return "NET_IP_REMOTE";
    case NET_IP_GATEWAY:
        return "NET_GATEWAY";
    case NET_IP_LOCAL:
        return "NET_IP_LOCAL";
    case NET_IP_MASK:
        return "NET_IP_MASK";
    case NET_PORT_LOCAL:
        return "NET_PORT_LOCAL";
    case NET_PORT_REMOTE:
        return "NET_PORT_REMOTE";
    default:
        return "NET_UNKNOWN";
    }
}

static void print_net_set_settings_event(const event_t* event)
{
    if (event == NULL)
    {
        return;
    }

    const event_queue_data_t* payload = NULL;
    event_queue_data_t        inline_payload;

    if (event->is_inline)
    {
        if (event->data_len < sizeof(event_queue_data_t))
        {
            printf("EV_SET_NET_SETTINGS: invalid data_len=%u\r\n", (unsigned)event->data_len);
            return;
        }
        memcpy(&inline_payload, event->data.bytes, sizeof(inline_payload));
        payload = &inline_payload;
    }
    else
    {
        if (event->data.ptr == NULL || event->data_len < sizeof(event_queue_data_t))
        {
            printf("EV_SET_NET_SETTINGS: invalid data ptr/len\r\n");
            return;
        }
        payload = (const event_queue_data_t*)event->data.ptr;
    }

    printf("EV_SET_NET_SETTINGS: id=%s\r\n", net_setting_id_name(payload->id));
    switch (payload->id)
    {
    case NET_IP_REMOTE:
    case NET_IP_LOCAL:
    case NET_IP_GATEWAY:
        printf("  ip=%u.%u.%u.%u\r\n", payload->value.ip[0], payload->value.ip[1],
               payload->value.ip[2], payload->value.ip[3]);
        break;
    case NET_IP_MASK:
        printf("  subnetmask=%u.%u.%u.%u\r\n", payload->value.ip[0], payload->value.ip[1],
               payload->value.ip[2], payload->value.ip[3]);
        break;
    case NET_PORT_LOCAL:
    case NET_PORT_REMOTE:
        printf("  port=%u\r\n", payload->value.port);
        break;
    default:
        printf("  raw: %02X %02X %02X %02X %02X %02X\r\n", payload->value.ip[0],
               payload->value.ip[1], payload->value.ip[2], payload->value.ip[3],
               (uint8_t)(payload->value.port >> 8), (uint8_t)(payload->value.port & 0xFF));
        break;
    }
}

static void print_net_get_settings_event(const event_t* event)
{
    if (event == NULL)
    {
        return;
    }

    const event_queue_data_t* payload = NULL;
    event_queue_data_t        inline_payload;

    if (event->is_inline)
    {
        if (event->data_len < sizeof(event_queue_data_t))
        {
            printf("EV_GET_NET_SETTINGS: invalid data_len=%u\r\n", (unsigned)event->data_len);
            return;
        }
        memcpy(&inline_payload, event->data.bytes, sizeof(inline_payload));
        payload = &inline_payload;
    }
    else
    {
        if (event->data.ptr == NULL || event->data_len < sizeof(event_queue_data_t))
        {
            printf("EV_GET_NET_SETTINGS: invalid data ptr/len\r\n");
            return;
        }
        payload = (const event_queue_data_t*)event->data.ptr;
    }

    printf("EV_GET_NET_SETTINGS: id=%s\r\n", net_setting_id_name(payload->id));
    switch (payload->id)
    {
    case NET_IP_REMOTE:
    case NET_IP_LOCAL:
    case NET_IP_GATEWAY:
        printf("  ip=%u.%u.%u.%u\r\n", payload->value.ip[0], payload->value.ip[1],
               payload->value.ip[2], payload->value.ip[3]);
        break;
    case NET_PORT_LOCAL:
    case NET_PORT_REMOTE:
        printf("  port=%u\r\n", payload->value.port);
        break;
    default:
        printf("  raw: %02X %02X %02X %02X %02X %02X\r\n", payload->value.ip[0],
               payload->value.ip[1], payload->value.ip[2], payload->value.ip[3],
               (uint8_t)(payload->value.port >> 8), (uint8_t)(payload->value.port & 0xFF));
        break;
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
            switch (event_item.type)
            {
            case EV_CLI_LINE:
            {
                const char* cli_line = NULL;
                if (event_get_payload_ptr(&event_item, 1, (const void**)&cli_line))
                {
                    handle_cli_line(cli_line);
                }
                app->need_prompt = true;
                break;
            }
            case EV_UDP_RX:
                LOG_DEBUG("tx_queue_enqueue_udp_frame: %d\r\n",
                          tx_queue_enqueue_udp_frame(&app->tx_queue, &app->rx_frame_buffer));
                memset(app->rx_frame_buffer.payload, 0, app->rx_frame_buffer.length);
                app->rx_frame_buffer.length = 0;
                app->need_prompt            = true;
                break;
            case EV_UDP_TX:
            {
                const UDP_FRAME_T* tx_frame = NULL;
                if (event_get_payload_ptr(&event_item, sizeof(*tx_frame), (const void**)&tx_frame))
                {
                    w5500_udp_tx(&app->destination_config, tx_frame);
                }
                app->need_prompt = true;
                break;
            }
            case EV_HDLC_DECODE:
            {
                const HDLC_FRAME_T* hdlc_frame = NULL;
                if (event_get_payload_ptr(&event_item, sizeof(*hdlc_frame),
                                          (const void**)&hdlc_frame))
                {
                    app->tx_frame_buffer.length = hdlc_frame->length;
                    PRINT_FRAME_HEX("Frame: ", hdlc_frame->payload, hdlc_frame->length);
                    hdlc_decode(hdlc_frame, app->tx_frame_buffer.payload, TX_BUF_SIZE,
                                &(app->tx_frame_buffer.length));
                    memset(hdlc_frame->payload, 0, hdlc_frame->length);
                    hdlc_sync_acc_init(&app->accumulator, HDLC_FLAG_BYTE);
                    event_t hdlc_frame_event = {.type     = EV_UDP_TX,
                                                .data.ptr = &app->tx_frame_buffer,
                                                .data_len = sizeof(app->tx_frame_buffer)};
                    event_queue_push(&hdlc_frame_event);
                }
                break;
            }
            case EV_SAVE_CONFIG:
                LOG_INFO("Storing persistent config in flash.\r\n");
                wizchip_getnetinfo(&(app->net_config.net_info));
                app->persistent_config.log_level     = current_log_level;
                app->persistent_config.v24_config    = app->v24_config;
                app->persistent_config.net_config    = app->net_config;
                app->persistent_config.local_config  = app->local_config;
                app->persistent_config.remote_config = app->destination_config;
                config_write(&app->persistent_config);
                LOG_INFO("Config stored. Dumping for checking:\r\n");
                dump_config();
                printf("\r\n> ");
                break;
            case EV_WIPE_CONFIG:
                LOG_INFO("Wiping persistent config in flash.\r\n");
                config_wipe();
                LOG_INFO("Config wiped. Dumping for checking:\r\n");
                dump_config();
                printf("\r\n> ");
                break;
            case EV_SET_NET_SETTINGS:
            {
                if (current_log_level == LOG_LEVEL_DEBUG)
                {
                    print_net_set_settings_event(&event_item);
                }
                const event_queue_data_t* payload = NULL;
                if (!event_get_payload_ptr(&event_item, sizeof(*payload), (const void**)&payload))
                {
                    break;
                }
                switch (payload->id)
                {
                case NET_IP_REMOTE:
                    memcpy(app->destination_config.ip_address, payload->value.ip, 4);
                    break;
                case NET_IP_LOCAL:
                {
                    memcpy(app->local_config.ip_address, payload->value.ip, 4);
                    wiz_NetInfo net_info;
                    wizchip_getnetinfo(&net_info);
                    memcpy(net_info.ip, payload->value.ip, 4);
                    wizchip_setnetinfo(&net_info);
                    break;
                }
                case NET_IP_MASK:
                {
                    wiz_NetInfo net_info;
                    wizchip_getnetinfo(&net_info);
                    memcpy(net_info.sn, payload->value.ip, 4);
                    wizchip_setnetinfo(&net_info);
                    break;
                }
                case NET_IP_GATEWAY:
                {
                    wiz_NetInfo net_info;
                    wizchip_getnetinfo(&net_info);
                    memcpy(net_info.gw, payload->value.ip, 4);
                    wizchip_setnetinfo(&net_info);
                    app->need_prompt = true;
                    break;
                }
                case NET_PORT_LOCAL:
                    app->local_config.port = payload->value.port;
                    w5500_reconfigure_udp_socket(&app->local_config);
                    app->need_prompt = true;
                    break;
                case NET_PORT_REMOTE:
                    app->destination_config.port = payload->value.port;
                    w5500_reconfigure_udp_socket(&app->destination_config);
                    app->need_prompt = true;
                    break;
                default:
                    break;
                }

                // For now dumbly always save after a change of settings.
                // This is nto the best idea since it degrades flash lifetime.
                event_t save_event = {.type = EV_SAVE_CONFIG, .data = NULL, .data_len = 0};
                event_queue_push(&save_event);
                break;
            }
            case EV_GET_NET_SETTINGS:
            {
                if (current_log_level == LOG_LEVEL_DEBUG)
                {
                    print_net_get_settings_event(&event_item);
                }
                const event_queue_data_t* payload = NULL;
                if (!event_get_payload_ptr(&event_item, sizeof(*payload), (const void**)&payload))
                {
                    break;
                }
                switch (payload->id)
                {
                case NET_IP_REMOTE:
                    printf("NET_IP_REMOTE: %d.%d.%d.%d\r\n", app->destination_config.ip_address[0],
                           app->destination_config.ip_address[1],
                           app->destination_config.ip_address[2],
                           app->destination_config.ip_address[3]);
                    break;
                case NET_IP_LOCAL:
                case NET_IP_MASK:
                case NET_IP_GATEWAY:
                {
                    wiz_NetInfo net_info;
                    wizchip_getnetinfo(&net_info);
                    printf("ip=%u.%u.%u.%u sn=%u.%u.%u.%u gw=%u.%u.%u.%u\r\n", net_info.ip[0],
                           net_info.ip[1], net_info.ip[2], net_info.ip[3], net_info.sn[0],
                           net_info.sn[1], net_info.sn[2], net_info.sn[3], net_info.gw[0],
                           net_info.gw[1], net_info.gw[2], net_info.gw[3]);
                    break;
                }
                case NET_PORT_LOCAL:
                    printf("NET_PORT_LOCAL: %d\r\n", app->local_config.port);
                    break;
                case NET_PORT_REMOTE:
                    printf("NET_PORT_REMOTE: %d\r\n", app->destination_config.port);
                    break;
                default:
                    break;
                }
                break;
            }
            break;
            case EV_SET_V24_SETTINGS:
            {
                const event_queue_data_t* payload = NULL;
                if (!event_get_payload_ptr(&event_item, sizeof(*payload), (const void**)&payload))
                {
                    break;
                }
                switch (payload->id)
                {
                case V24_BAUDRATE:
                    memcpy(&app->v24_config.baudrate, &payload->value.baudrate,
                           sizeof(V24_BAUDRATE_T));
                    tx_clock_init(pio0, 0, app->v24_config.baudrate,
                                  &(app->v24_config.polarities.tx_polarities));
                    rx_clock_init(pio0, 1, &(app->v24_config.polarities.rx_polarities));
                    app->need_prompt = true;
                    break;
                case V24_POLARITIES:
                    memcpy(&app->v24_config.polarities, &payload->value.polarities,
                           sizeof(V24_POLARITIES_T));

                    tx_clock_init(pio0, 0, app->v24_config.baudrate,
                                  &(app->v24_config.polarities.tx_polarities));
                    rx_clock_init(pio0, 1, &(app->v24_config.polarities.rx_polarities));
                    app->need_prompt = true;
                    break;
                default:
                    break;
                }
                // For now dumbly always save after a change of settings.
                // This is nto the best idea since it degrades flash lifetime.
                event_t save_event = {.type = EV_SAVE_CONFIG, .data = NULL, .data_len = 0};
                event_queue_push(&save_event);
                break;
            }
            case EV_GET_V24_SETTINGS:
            {
                const event_queue_data_t* payload = NULL;
                if (!event_get_payload_ptr(&event_item, sizeof(*payload), (const void**)&payload))
                {
                    break;
                }
                switch (payload->id)
                {
                case V24_BAUDRATE:
                    printf("V24_BAUDRATE: %d\r\n", app->v24_config.baudrate);
                    app->need_prompt = true;
                    break;
                case V24_POLARITIES:
                    print_v24_polarities(&app->v24_config.polarities);
                    app->need_prompt = true;
                    break;
                default:
                    break;
                }
                break;
            }
            default:
                break;
            }
        }
        print_prompt(app);
        sleep_us(MAIN_LOOP_SLEEP_US);
    }
}
