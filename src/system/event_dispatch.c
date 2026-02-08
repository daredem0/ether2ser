
// Related headers
#include "system/event_dispatch.h"

// Standard library headers
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Library Headers
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"

// Project Headers
#include "drivers/pio_tx_rx_driver.h"
#include "drivers/tx_queue.h"
#include "drivers/v24_config.h"
#include "drivers/w5500_driver.h"
#include "protocol/hdlc_common.h"
#include "protocol/hdlc_decoder.h"
#include "protocol/hdlc_sync.h"
#include "system/app_context.h"
#include "system/cli_commands.h"
#include "system/cli_usb_cdc.h"
#include "system/common.h"
#include "system/event_queue.h"
#include "system/persistent_config.h"

// Generated headers

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

static void request_save_config(void)
{
    event_t save_event = {
        .type      = EV_SAVE_CONFIG,
        .data.ptr  = NULL,
        .data_len  = 0,
        .is_inline = false,
    };
    event_queue_push(&save_event);
}

static void ev_set_net_settings(const event_queue_data_t* payload, app_ctx_t* app)
{

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
}

static void ev_get_net_settings(const event_queue_data_t* payload, app_ctx_t* app)
{
    switch (payload->id)
    {
    case NET_IP_REMOTE:
        printf("NET_IP_REMOTE: %d.%d.%d.%d\r\n", app->destination_config.ip_address[0],
               app->destination_config.ip_address[1], app->destination_config.ip_address[2],
               app->destination_config.ip_address[3]);
        break;
    case NET_IP_LOCAL:
    case NET_IP_MASK:
    case NET_IP_GATEWAY:
    {
        wiz_NetInfo net_info;
        wizchip_getnetinfo(&net_info);
        printf("ip=%u.%u.%u.%u sn=%u.%u.%u.%u gw=%u.%u.%u.%u\r\n", net_info.ip[0], net_info.ip[1],
               net_info.ip[2], net_info.ip[3], net_info.sn[0], net_info.sn[1], net_info.sn[2],
               net_info.sn[3], net_info.gw[0], net_info.gw[1], net_info.gw[2], net_info.gw[3]);
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
}

static void ev_set_v24_settings(const event_queue_data_t* payload, app_ctx_t* app)
{
    switch (payload->id)
    {
    case V24_BAUDRATE:
        reinit_v24_config(&app->v24_config, payload->value.baudrate);
        tx_clock_update_settings(pio0, 0, app->v24_config.baudrate,
                                 &(app->v24_config.polarities.tx_polarities));
        app->need_prompt = true;
        break;
    case V24_POLARITIES:
        memcpy(&app->v24_config.polarities, &payload->value.polarities, sizeof(V24_POLARITIES_T));
        tx_clock_update_settings(pio0, 0, app->v24_config.baudrate,
                                 &(app->v24_config.polarities.tx_polarities));
        rx_clock_update_settings(pio0, 1, &(app->v24_config.polarities.rx_polarities));
        app->need_prompt = true;
        break;
    default:
        break;
    }
}

static void ev_get_v24_settings(const event_queue_data_t* payload, app_ctx_t* app)
{
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
}

void event_dispatch(event_t* event, app_ctx_t* app)
{
    switch (event->type)
    {
    case EV_STATUS:
    {
        uint64_t frame_gap = 0;
        if (app->stats.hdlc_tx_frames > app->stats.hdlc_frame_ready)
        {
            frame_gap = app->stats.hdlc_tx_frames - app->stats.hdlc_frame_ready;
        }
        uint64_t tx_gap = 0;
        if (app->stats.hdlc_frame_ready > app->stats.udp_tx_frames)
        {
            tx_gap = app->stats.hdlc_frame_ready - app->stats.udp_tx_frames;
        }

        printf("PIPE STATS\r\n");
        printf("  RX/TX      : udp_rx=%" PRIu64 " hdlc_tx=%" PRIu64 " hdlc_rx=%" PRIu64
               " udp_tx=%" PRIu64 "\r\n",
               app->stats.udp_rx_frames, app->stats.hdlc_tx_frames, app->stats.hdlc_frame_ready,
               app->stats.udp_tx_frames);
        printf("  Decode     : frame_ready=%" PRIu64 " ok=%" PRIu64 " fail=%" PRIu64 "\r\n",
               app->stats.hdlc_frame_ready, app->stats.hdlc_decode_ok, app->stats.hdlc_decode_fail);
        printf("  Pipeline   : tx->ready_gap=%" PRIu64 " ready->udp_gap=%" PRIu64 "\r\n", frame_gap,
               tx_gap);
        printf("  Serial RX  : bytes=%" PRIu64 "\r\n", app->stats.serial_rx_bytes);
        printf("  Serial TX  : wire_bytes=%" PRIu64 "\r\n", app->tx_queue.tx_wire_bytes);
        printf("  Sync Wait  : syncing=%" PRIu64 " synced=%" PRIu64 "\r\n",
               app->stats.sync_lookahead_wait_syncing, app->stats.sync_lookahead_wait_synced);
        printf("  Sync Maint : consume=%" PRIu64 " hardcap_events=%" PRIu64
               " hardcap_bytes=%" PRIu64 "\r\n",
               app->stats.sync_candidate_consume, app->stats.sync_hardcap_drop_events,
               app->stats.sync_hardcap_drop_bytes);
        printf("  Accum      : pos=%zu proc=%zu state=%d off=%u cand_valid=%d cand_end=%zu\r\n",
               app->accumulator.position, app->accumulator.processed, (int)app->accumulator.state,
               app->accumulator.bit_offset, app->accumulator.candidate_valid ? 1 : 0,
               app->accumulator.candidate_end);
        printf("  Queues     : tx_count=%zu/%zu tx_active=%d recon_len=%zu\r\n",
               app->tx_queue.queue_buffer.count, app->tx_queue.queue_buffer.capacity,
               (app->tx_queue.current_entry.offset < app->tx_queue.current_entry.frame.length) ? 1
                                                                                               : 0,
               app->reconstructed_frame.length);
        printf("PIO SM0 stalled: %d\n", (pio0->fdebug >> PIO_FDEBUG_TXSTALL_LSB) & 1);

        app->need_prompt = true;
    }
    break;
    case EV_CLI_LINE:
    {
        const char* cli_line = NULL;
        if (event_get_payload_ptr(event, 1, (const void**)&cli_line))
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
        if (event_get_payload_ptr(event, sizeof(*tx_frame), (const void**)&tx_frame))
        {
            w5500_udp_tx(&app->destination_config, tx_frame);
        }
        app->need_prompt = true;
        break;
    }
    case EV_HDLC_DECODE:
    {
        const HDLC_FRAME_T* hdlc_frame = NULL;
        if (event_get_payload_ptr(event, sizeof(*hdlc_frame), (const void**)&hdlc_frame))
        {
            app->tx_frame_buffer.length = 0;
            PRINT_FRAME_HEX("Frame: ", hdlc_frame->payload, hdlc_frame->length);
            if (hdlc_decode(hdlc_frame, app->tx_frame_buffer.payload, TX_BUF_SIZE,
                            &(app->tx_frame_buffer.length), true))
            {
                event_t hdlc_frame_event = {.type     = EV_UDP_TX,
                                            .data.ptr = &app->tx_frame_buffer,
                                            .data_len = sizeof(app->tx_frame_buffer)};
                event_queue_push(&hdlc_frame_event);
            }
            memset(hdlc_frame->payload, 0, hdlc_frame->length);
            hdlc_sync_acc_init(&app->accumulator, HDLC_FLAG_BYTE);
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
            print_net_set_settings_event(event);
        }
        const event_queue_data_t* payload = NULL;
        if (!event_get_payload_ptr(event, sizeof(*payload), (const void**)&payload))
        {
            break;
        }
        ev_set_net_settings(payload, app);
        // For now dumbly always save after a change of settings.
        // This is not the best idea since it degrades flash lifetime.
        request_save_config();
        break;
    }
    case EV_GET_NET_SETTINGS:
    {
        if (current_log_level == LOG_LEVEL_DEBUG)
        {
            print_net_get_settings_event(event);
        }
        const event_queue_data_t* payload = NULL;
        if (!event_get_payload_ptr(event, sizeof(*payload), (const void**)&payload))
        {
            break;
        }

        ev_get_net_settings(payload, app);
        break;
    }
    case EV_SET_V24_SETTINGS:
    {
        const event_queue_data_t* payload = NULL;
        if (!event_get_payload_ptr(event, sizeof(*payload), (const void**)&payload))
        {
            break;
        }
        ev_set_v24_settings(payload, app);
        // For now dumbly always save after a change of settings.
        // This is not the best idea since it degrades flash lifetime.
        request_save_config();
        break;
    }
    case EV_GET_V24_SETTINGS:
    {
        const event_queue_data_t* payload = NULL;
        if (!event_get_payload_ptr(event, sizeof(*payload), (const void**)&payload))
        {
            break;
        }
        ev_get_v24_settings(payload, app);
        break;
    }
    default:
        break;
    }
}
