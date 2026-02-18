/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/event_dispatch.c
 * Purpose: Event dispatcher handlers for CLI, status, and configuration events.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "system/event_dispatch.h"

// Standard library headers
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Library Headers
#include "pico/time.h"
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"

// Project Headers
#include "drivers/pio_tx_rx_driver.h"
#include "drivers/tx_queue.h"
#include "drivers/v24_config.h"
#include "drivers/w5500_driver.h"
#include "platform/watchdog.h"
#include "protocol/hdlc_common.h"
#include "protocol/hdlc_decoder.h"
#include "protocol/hdlc_sync.h"
#include "system/app_context.h"
#include "system/cli_commands.h"
#include "system/cli_usb_cdc.h"
#include "system/common.h"
#include "system/error.h"
#include "system/event_queue.h"
#include "system/persistent_config.h"

// Generated headers

static void print_v24_polarities(const V24_POLARITIES_T* polarities)
{
    if (polarities == NULL)
    {
        LOG_PLAIN("polarities: <null>\r\n");
        return;
    }

    LOG_PLAIN("polarities: ");
    bool first = true;
#define ADD(name, flag)                                \
    do                                                 \
    {                                                  \
        if (flag)                                      \
        {                                              \
            LOG_PLAIN("%s%s", first ? "" : ",", name); \
            first = false;                             \
        }                                              \
    } while (0)

    ADD("txd", polarities->tx_polarities.txd_inverted);
    ADD("txc", polarities->tx_polarities.txc_inverted);
    ADD("cts", polarities->tx_polarities.cts_inverted);
    ADD("rts", polarities->tx_polarities.rts_inverted);
    ADD("dtr", polarities->tx_polarities.dtr_inverted);
    ADD("rxd", polarities->rx_polarities.rxd_inverted);
    ADD("rxc", polarities->rx_polarities.rxc_inverted);
    ADD("dcd", polarities->rx_polarities.dcd_inverted);

    if (first)
    {
        LOG_PLAIN("<none>");
    }
    LOG_PLAIN("\r\n");
}

static const char* net_setting_id_name(event_queue_data_types_t event_id)
{
    switch (event_id)
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

static const char* hdlc_sync_state_name(HDLC_SYNC_STATE_T state)
{
    switch (state)
    {
    case HDLC_SYNC_STATE_HUNTING:
        return "HUNTING";
    case HDLC_SYNC_STATE_SYNCING:
        return "SYNCING";
    case HDLC_SYNC_STATE_SYNCED:
        return "SYNCED";
    default:
        return "UNKNOWN";
    }
}

static void print_net_settings_event(const event_t* event)
{
    if (event == NULL)
    {
        return;
    }

    const event_queue_data_t* payload = NULL;
    if (!event_get_payload_ptr(event, sizeof(*payload), (const void**)&payload))
    {
        return;
    }

    LOG_DEBUG("EV_NET_SETTINGS: id=%s\r\n", net_setting_id_name(payload->id));
    switch (payload->id)
    {
    case NET_IP_REMOTE:
    case NET_IP_LOCAL:
    case NET_IP_GATEWAY:
        LOG_DEBUG("  ip=%u.%u.%u.%u\r\n", payload->value.ip[0], payload->value.ip[1],
                  payload->value.ip[2], payload->value.ip[3]);
        break;
    case NET_IP_MASK:
        LOG_DEBUG("  subnetmask=%u.%u.%u.%u\r\n", payload->value.ip[0], payload->value.ip[1],
                  payload->value.ip[2], payload->value.ip[3]);
        break;
    case NET_PORT_LOCAL:
    case NET_PORT_REMOTE:
        LOG_DEBUG("  port=%u\r\n", payload->value.port);
        break;
    default:
        LOG_DEBUG("  raw: %02X %02X %02X %02X %02X %02X\r\n", payload->value.ip[0],
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

typedef struct
{
    bool changed;
    bool reboot_required;
} apply_result_t;

static bool ev_set_net_settings(const event_queue_data_t* payload, app_ctx_t* app)
{
    bool        changed = false;
    e2s_error_t err;
    switch (payload->id)
    {
    case NET_IP_REMOTE:
        if (memcmp(app->destination_config.ip_address, payload->value.ip, 4) != 0)
        {
            memcpy(app->destination_config.ip_address, payload->value.ip, 4);
            changed = true;
        }
        break;
    case NET_IP_LOCAL:
    {
        wiz_NetInfo net_info;
        wizchip_getnetinfo(&net_info);
        if (memcmp(net_info.ip, payload->value.ip, 4) != 0)
        {
            memcpy(app->local_config.ip_address, payload->value.ip, 4);
            memcpy(net_info.ip, payload->value.ip, 4);
            wizchip_setnetinfo(&net_info);
            changed = true;
        }
        break;
    }
    case NET_IP_MASK:
    {
        wiz_NetInfo net_info;
        wizchip_getnetinfo(&net_info);
        if (memcmp(net_info.sn, payload->value.ip, 4) != 0)
        {
            memcpy(net_info.sn, payload->value.ip, 4);
            wizchip_setnetinfo(&net_info);
            changed = true;
        }
        break;
    }
    case NET_IP_GATEWAY:
    {
        wiz_NetInfo net_info;
        wizchip_getnetinfo(&net_info);
        if (memcmp(net_info.gw, payload->value.ip, 4) != 0)
        {
            memcpy(net_info.gw, payload->value.ip, 4);
            wizchip_setnetinfo(&net_info);
            changed = true;
        }
        break;
    }
    case NET_PORT_LOCAL:
        if (app->local_config.port != payload->value.port)
        {
            app->local_config.port = payload->value.port;
            if ((err = w5500_reconfigure_udp_socket(&app->local_config)) != E2S_OK)
            {
                fatal_panic(err);
            }
            changed = true;
        }
        break;
    case NET_PORT_REMOTE:
        if (app->destination_config.port != payload->value.port)
        {
            app->destination_config.port = payload->value.port;
            if ((err = w5500_reconfigure_udp_socket(&app->destination_config)) != E2S_OK)
            {
                fatal_panic(err);
            }
            changed = true;
        }
        break;
    default:
        break;
    }
    app->need_prompt = true;
    return changed;
}

static void ev_get_net_settings(const event_queue_data_t* payload, const app_ctx_t* app)
{
    switch (payload->id)
    {
    case NET_IP_REMOTE:
        LOG_PLAIN("NET_IP_REMOTE: %d.%d.%d.%d\r\n", app->destination_config.ip_address[0],
                  app->destination_config.ip_address[1], app->destination_config.ip_address[2],
                  app->destination_config.ip_address[3]);
        break;
    case NET_IP_LOCAL:
    case NET_IP_MASK:
    case NET_IP_GATEWAY:
    {
        wiz_NetInfo net_info;
        wizchip_getnetinfo(&net_info);
        LOG_PLAIN("ip=%u.%u.%u.%u sn=%u.%u.%u.%u gw=%u.%u.%u.%u\r\n", net_info.ip[0],
                  net_info.ip[1], net_info.ip[2], net_info.ip[3], net_info.sn[0], net_info.sn[1],
                  net_info.sn[2], net_info.sn[3], net_info.gw[0], net_info.gw[1], net_info.gw[2],
                  net_info.gw[3]);
        break;
    }
    case NET_PORT_LOCAL:
        LOG_PLAIN("NET_PORT_LOCAL: %d\r\n", app->local_config.port);
        break;
    case NET_PORT_REMOTE:
        LOG_PLAIN("NET_PORT_REMOTE: %d\r\n", app->destination_config.port);
        break;
    default:
        break;
    }
}

static apply_result_t ev_set_v24_settings(const event_queue_data_t* payload, app_ctx_t* app)
{
    apply_result_t result = {0};
    switch (payload->id)
    {
    case V24_BAUDRATE:
        if (app->v24_config.baudrate != payload->value.baudrate)
        {
            reinit_v24_config(&app->v24_config, payload->value.baudrate);
            tx_clock_update_settings(&app->v24_config);
            result.changed = true;
        }
        app->need_prompt = true;
        break;
    case V24_POLARITIES:
        if (memcmp(&app->v24_config.polarities, &payload->value.polarities,
                   sizeof(V24_POLARITIES_T)) != 0)
        {
            memcpy(&app->v24_config.polarities, &payload->value.polarities,
                   sizeof(V24_POLARITIES_T));
            tx_clock_update_settings(&app->v24_config);
            rx_clock_update_settings(&(app->v24_config.polarities.rx_polarities));
            result.changed = true;
        }
        app->need_prompt = true;
        break;
    case V24_CLOCK_MODE:
    {
        bool external_clock = payload->value.v24_clock_mode;

        if (app->v24_config.external_clock != external_clock)
        {
            if (!tx_queue_is_empty(&app->tx_queue))
            {
                LOG_ERROR("Cannot change clock mode during ongoing transmission!\r\n");
                break;
            }
            LOG_PLAIN("Switching to %s mode.\r\n", external_clock ? "external" : "internal");
            app->v24_config.external_clock = external_clock;
            result.changed                 = true;
            result.reboot_required         = true;
        }
        else
        {
            LOG_INFO("Already in %s mode.\r\n", external_clock ? "external" : "internal");
            break;
        }
        app->need_prompt = true;
        break;
    }
    default:
        break;
    }
    return result;
}

static void ev_get_v24_settings(const event_queue_data_t* payload, app_ctx_t* app)
{
    switch (payload->id)
    {
    case V24_BAUDRATE:
        LOG_PLAIN("V24_BAUDRATE: %d\r\n", app->v24_config.baudrate);
        app->need_prompt = true;
        break;
    case V24_POLARITIES:
        print_v24_polarities(&app->v24_config.polarities);
        app->need_prompt = true;
        break;
    case V24_CLOCK_MODE:
        LOG_PLAIN("V24 TX Clock: %s\r\n", app->v24_config.external_clock ? "external (XCK Pin 15)"
                                                                         : "internal (TCK Pin 17)");
        app->need_prompt = true;
        break;
    default:
        break;
    }
}

typedef struct
{
    uint64_t frame_gap;
    uint64_t tx_gap;
    uint64_t udp_rx_rate_fps;
    uint64_t hdlc_rx_rate_fps;
    uint64_t udp_tx_rate_fps;
    uint64_t decode_fail_rate;
    uint64_t serial_rx_rate_bps;
} ev_status_stats_t;

static void ev_status(app_ctx_t* app, ev_status_stats_t* status_stats)
{
    status_stats->frame_gap = 0;
    if (app->stats.hdlc_tx_frames > app->stats.hdlc_frame_ready)
    {
        status_stats->frame_gap = app->stats.hdlc_tx_frames - app->stats.hdlc_frame_ready;
    }
    status_stats->tx_gap = 0;
    if (app->stats.hdlc_frame_ready > app->stats.udp_tx_frames)
    {
        status_stats->tx_gap = app->stats.hdlc_frame_ready - app->stats.udp_tx_frames;
    }

    static uint64_t prev_udp_rx_frames    = 0U;
    static uint64_t prev_hdlc_frame_ready = 0U;
    static uint64_t prev_udp_tx_frames    = 0U;
    static uint64_t prev_hdlc_decode_fail = 0U;
    static uint64_t prev_serial_rx_bytes  = 0U;
    uint32_t        report_now_ms         = to_ms_since_boot(get_absolute_time());
    uint32_t        elapsed_ms            = 0U;
    if (app->stats.last_report_ms != 0U)
    {
        elapsed_ms = report_now_ms - app->stats.last_report_ms;
    }
    if (elapsed_ms == 0U)
    {
        elapsed_ms = 1U;
    }

    uint64_t d_udp_rx_frames    = app->stats.udp_rx_frames - prev_udp_rx_frames;
    uint64_t d_hdlc_frame_ready = app->stats.hdlc_frame_ready - prev_hdlc_frame_ready;
    uint64_t d_udp_tx_frames    = app->stats.udp_tx_frames - prev_udp_tx_frames;
    uint64_t d_decode_fail      = app->stats.hdlc_decode_fail - prev_hdlc_decode_fail;
    uint64_t d_serial_rx_bytes  = app->stats.serial_rx_bytes - prev_serial_rx_bytes;

    status_stats->udp_rx_rate_fps    = (d_udp_rx_frames * 1000U) / elapsed_ms;
    status_stats->hdlc_rx_rate_fps   = (d_hdlc_frame_ready * 1000U) / elapsed_ms;
    status_stats->udp_tx_rate_fps    = (d_udp_tx_frames * 1000U) / elapsed_ms;
    status_stats->decode_fail_rate   = (d_decode_fail * 1000U) / elapsed_ms;
    status_stats->serial_rx_rate_bps = (d_serial_rx_bytes * 1000U) / elapsed_ms;

    prev_udp_rx_frames        = app->stats.udp_rx_frames;
    prev_hdlc_frame_ready     = app->stats.hdlc_frame_ready;
    prev_udp_tx_frames        = app->stats.udp_tx_frames;
    prev_hdlc_decode_fail     = app->stats.hdlc_decode_fail;
    prev_serial_rx_bytes      = app->stats.serial_rx_bytes;
    app->stats.last_report_ms = report_now_ms;
}

static void print_status_event(app_ctx_t* app, ev_status_stats_t* status_stats)
{

    LOG_PLAIN("status: ok\r\n");
    LOG_PLAIN("Current Baudrate estimation on pin %d: %.1f Hz\r\n", V24_RXC,
              baudrate_estimator_get_current_estimation(V24_RXC));
    if (app->v24_config.external_clock)
    {
        LOG_PLAIN("Current Baudrate estimation on pin %d: %.1f Hz\r\n", V24_TXC_DCE,
                  baudrate_estimator_get_current_estimation(V24_TXC_DCE));
    }
    LOG_PLAIN("PIPE STATS\r\n");
    LOG_PLAIN("  Traffic\r\n");
    LOG_PLAIN("    Frames    : udp_rx=%" PRIu64 "  hdlc_tx=%" PRIu64 "  hdlc_rx=%" PRIu64
              "  udp_tx=%" PRIu64 "\r\n",
              app->stats.udp_rx_frames, app->stats.hdlc_tx_frames, app->stats.hdlc_frame_ready,
              app->stats.udp_tx_frames);
    LOG_PLAIN("    Backlog   : tx->ready_gap=%" PRIu64 "  ready->udp_gap=%" PRIu64 "\r\n",
              status_stats->frame_gap, status_stats->tx_gap);
    LOG_PLAIN("    Serial    : rx_bytes=%" PRIu64 "  tx_bytes=%" PRIu64 "\r\n",
              app->stats.serial_rx_bytes, app->tx_queue.tx_wire_bytes);
    LOG_PLAIN("    Rates     : udp_rx=%" PRIu64 "/s  hdlc_rx=%" PRIu64 "/s  udp_tx=%" PRIu64
              "/s  fail=%" PRIu64 "/s  rx_bytes=%" PRIu64 "/s\r\n",
              status_stats->udp_rx_rate_fps, status_stats->hdlc_rx_rate_fps,
              status_stats->udp_tx_rate_fps, status_stats->decode_fail_rate,
              status_stats->serial_rx_rate_bps);

    LOG_PLAIN("  Decode / Sync\r\n");
    LOG_PLAIN("    Decode    : frame_ready=%" PRIu64 "  ok=%" PRIu64 "  fail=%" PRIu64 "\r\n",
              app->stats.hdlc_frame_ready, app->stats.hdlc_decode_ok, app->stats.hdlc_decode_fail);
    LOG_PLAIN("    FailReason: invalid=%" PRIu64 "  short=%" PRIu64 "  long=%" PRIu64
              "  unstuff=%" PRIu64 "  crc=%" PRIu64 "\r\n",
              app->stats.decode_fail_invalid_frame, app->stats.decode_fail_too_short,
              app->stats.decode_fail_payload_too_long, app->stats.decode_fail_unstuff_error,
              app->stats.decode_fail_crc_mismatch);
    LOG_PLAIN("    SyncState : %s\r\n", hdlc_sync_state_name(app->accumulator.state));
    LOG_PLAIN("    SyncWait  : syncing=%" PRIu64 "  synced=%" PRIu64 "\r\n",
              app->stats.sync_lookahead_wait_syncing, app->stats.sync_lookahead_wait_synced);
    LOG_PLAIN("    SyncMaint : consume=%" PRIu64 "  hardcap_events=%" PRIu64
              "  hardcap_bytes=%" PRIu64 "\r\n",
              app->stats.sync_candidate_consume, app->stats.sync_hardcap_drop_events,
              app->stats.sync_hardcap_drop_bytes);
    LOG_PLAIN("    Resync    : idle=%" PRIu64 "  hard=%" PRIu64 "  no_progress=%" PRIu64 "\r\n",
              app->stats.resync_idle_timeout_count, app->stats.resync_hard_fail_count,
              app->stats.resync_no_progress_count);
    LOG_PLAIN(
        "    Accum     : pos=%zu  proc=%zu  state=%d  off=%u  cand_valid=%d  cand_end=%zu\r\n",
        app->accumulator.position, app->accumulator.processed, (int)app->accumulator.state,
        app->accumulator.bit_offset, app->accumulator.candidate_valid ? 1 : 0,
        app->accumulator.candidate_end);
    LOG_PLAIN("    RX Health : acc_pos_max=%" PRIu64 "  rx_fifo_stall=%" PRIu64
              "  rx_drop_acc_full=%" PRIu64 "  hunt_idle_drop=%" PRIu64 "\r\n",
              app->stats.accumulator_pos_max, app->stats.rx_fifo_stall_events,
              app->stats.serial_rx_drop_acc_full, app->stats.hunt_idle_drop_bytes);

    LOG_PLAIN("  Buffers\r\n");
    LOG_PLAIN("    TX Queue  : used=%zu/%zu  active=%d\r\n", app->tx_queue.queue_buffer.count,
              app->tx_queue.queue_buffer.capacity,
              (app->tx_queue.current_entry.offset < app->tx_queue.current_entry.frame.length) ? 1
                                                                                              : 0);
    LOG_PLAIN("    HighWater : tx=%" PRIu64 "  event=%" PRIu64 "  log=%" PRIu64 "\r\n",
              app->stats.tx_queue_used_max, app->stats.event_queue_used_max,
              app->stats.log_queue_used_max);
    LOG_PLAIN("    W5500 Buf : rx_no_room_events=%" PRIu64 "  tx_full_events=%" PRIu64 "\r\n",
              app->stats.udp_rx_buffer_full_counts, app->stats.udp_tx_buffer_full_counts);
    LOG_PLAIN("    Drops     : tx=%" PRIu64 "  event=%" PRIu64 "  log=%" PRIu64 "\r\n",
              app->stats.tx_queue_drop_frames, app->stats.event_queue_drop_events,
              app->stats.log_drop_lines);
    LOG_PLAIN("    Recons    : len=%zu\r\n", app->reconstructed_frame.length);
    LOG_PLAIN("    Throttle  : udp_rx_enter=%" PRIu64 "  udp_rx_skips=%" PRIu64 "\r\n",
              app->stats.udp_rx_throttle_enter, app->stats.udp_rx_throttle_skips);

    const v24_runtime_t* v24_runtime = get_v24_runtime();
    LOG_PLAIN(
        "    PIO TX    : stalled=%" PRIu32 "\r\n",
        (v24_runtime->tx_pio)
            ? ((v24_runtime->tx_pio->fdebug >>
                (PIO_FDEBUG_TXSTALL_LSB + v24_runtime->tx_sm)) & // NOLINT(misc-include-cleaner)
               1U)
            : 0);
}

void event_dispatch(const event_t* event, app_ctx_t* app)
{
    switch (event->type)
    {
    case EV_REBOOT:
    {
        reboot();
        break; // Compiler wants break because dosnt detect trap in reboot
    }
    case EV_STATUS:
    {
        ev_status_stats_t status_stats = {0};
        ev_status(app, &status_stats);
        print_status_event(app, &status_stats);
        app->need_prompt = true;
    }
    break;
    case EV_MEM:
        print_memory_usage();
        print_flash_usage();
        app->need_prompt = true;
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
        LOG_PLAIN("Storing persistent config in flash.\r\n");
        wizchip_getnetinfo(&(app->net_config.net_info));
        app->persistent_config.log_level     = get_loglevel();
        app->persistent_config.v24_config    = app->v24_config;
        app->persistent_config.net_config    = app->net_config;
        app->persistent_config.local_config  = app->local_config;
        app->persistent_config.remote_config = app->destination_config;
        config_write(&app->persistent_config);
        LOG_PLAIN("Config stored. Dumping for checking:\r\n");
        dump_config();
        app->need_prompt = true;
        break;
    case EV_WIPE_CONFIG:
        LOG_PLAIN("Wiping persistent config in flash.\r\n");
        config_wipe();
        LOG_PLAIN("Config wiped. Dumping for checking:\r\n");
        dump_config();
        app->need_prompt = true;
        break;
    case EV_SET_NET_SETTINGS:
    {
        if (get_loglevel() == LOG_LEVEL_DEBUG)
        {
            print_net_settings_event(event);
        }
        const event_queue_data_t* payload = NULL;
        if (!event_get_payload_ptr(event, sizeof(*payload), (const void**)&payload))
        {
            break;
        }
        if (ev_set_net_settings(payload, app))
        {
            request_save_config();
        }
        break;
    }
    case EV_GET_NET_SETTINGS:
    {
        if (get_loglevel() == LOG_LEVEL_DEBUG)
        {
            print_net_settings_event(event);
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
        apply_result_t set_result = ev_set_v24_settings(payload, app);
        if (set_result.changed)
        {
            request_save_config();
        }
        if (set_result.reboot_required)
        {
            event_t reboot_event = {
                .type      = EV_REBOOT,
                .data.ptr  = NULL,
                .data_len  = 0,
                .is_inline = false,
            };
            event_queue_push(&reboot_event);
        }
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
