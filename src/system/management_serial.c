#include "system/management_serial.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "drivers/gpio_control.h"
#include "platform/pinmap.h"
#include "protocol/hdlc_sync.h"
#include "system/common.h"
#include "system/management_app.h"
#include "system/management_catalog.h"
#include "system/management_cli.h"

static const char* loglevel_name(uint32_t level)
{
    switch (level)
    {
    case 0U:
        return "plain";
    case 1U:
        return "error";
    case 2U:
        return "info";
    case 3U:
        return "debug";
    case 4U:
        return "trace";
    default:
        return "unknown";
    }
}

static const char* decode_failure_reason_name(hdlc_decode_failure_reason_t reason)
{
    switch (reason)
    {
    case HDLC_DECODE_FAILURE_INVALID_FRAME:
        return "invalid";
    case HDLC_DECODE_FAILURE_TOO_SHORT:
        return "short";
    case HDLC_DECODE_FAILURE_PAYLOAD_TOO_LONG:
        return "long";
    case HDLC_DECODE_FAILURE_UNSTUFF_ERROR:
        return "unstuff";
    case HDLC_DECODE_FAILURE_CRC_MISMATCH:
        return "crc";
    case HDLC_DECODE_FAILURE_NONE:
    default:
        return "none";
    }
}

static const char* sync_state_name(int state)
{
    switch ((HDLC_SYNC_STATE_T)state)
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

#define INVERTED_SIGNALS_TEXT_SIZE 32U

static void emit_inverted_signals(const char* label, uint32_t mask)
{
    char signals[INVERTED_SIGNALS_TEXT_SIZE];
    if (!management_cli_format_inverted_signals(mask, signals, sizeof(signals)))
    {
        LOG_PLAIN("%sunknown\r\n", label);
        return;
    }
    LOG_PLAIN("%s%s\r\n", label, signals);
}

static void emit_scalar(const char* path, const management_value_t* value)
{
    switch (value->kind)
    {
    case MANAGEMENT_VALUE_MODE:
        LOG_PLAIN("Operating mode: %s\r\n", operating_mode_name(value->data.mode));
        break;
    case MANAGEMENT_VALUE_BOOL:
        if (strcmp(path, "v24.txclock") == 0)
        {
            LOG_PLAIN("V.24 TX clock: %s\r\n",
                      value->data.boolean ? "external" : "internal");
        }
        else
        {
            LOG_PLAIN("%s: %s\r\n", path, value->data.boolean ? "on" : "off");
        }
        break;
    case MANAGEMENT_VALUE_U16:
        LOG_PLAIN("%s: %u\r\n", path, (unsigned)value->data.u16);
        break;
    case MANAGEMENT_VALUE_U32:
        LOG_PLAIN("%s: %" PRIu32 "\r\n", path, value->data.u32);
        break;
    case MANAGEMENT_VALUE_SIGNAL_MASK:
        emit_inverted_signals("V.24 inverted signals: ", value->data.u32);
        break;
    case MANAGEMENT_VALUE_IPV4:
        LOG_PLAIN("%s: %u.%u.%u.%u\r\n", path, value->data.ipv4[0],
                  value->data.ipv4[1], value->data.ipv4[2],
                  value->data.ipv4[3]);
        break;
    case MANAGEMENT_VALUE_IPV4_CIDR:
        LOG_PLAIN("%s: %u.%u.%u.%u/%u\r\n", path,
                  value->data.ipv4_cidr.address[0],
                  value->data.ipv4_cidr.address[1],
                  value->data.ipv4_cidr.address[2],
                  value->data.ipv4_cidr.address[3],
                  value->data.ipv4_cidr.prefix_length);
        break;
    case MANAGEMENT_VALUE_GPIO:
    {
        const gpio_signal_info_t* signal =
            gpio_control_signal_at((size_t)value->data.gpio.signal);
        if (signal != NULL)
        {
            LOG_PLAIN("%s (GPIO %u): %s (%u)\r\n", signal->name,
                      signal->gpio_num,
                      value->data.gpio.level ? "HIGH" : "LOW",
                      value->data.gpio.level ? 1U : 0U);
        }
        break;
    }
    case MANAGEMENT_VALUE_TEXT:
        if (value->data.text != NULL)
        {
            if (strcmp(path, "system.version") == 0)
            {
                LOG_PLAIN("Firmware version: %s\r\n", value->data.text);
            }
            else
            {
                LOG_PLAIN("%s: %s\r\n", path, value->data.text);
            }
        }
        break;
    case MANAGEMENT_VALUE_NONE:
    default:
        break;
    }
}

static const char* help_access(uint8_t method_mask)
{
    const uint8_t get_set = MANAGEMENT_METHOD_MASK(MANAGEMENT_METHOD_GET) |
                            MANAGEMENT_METHOD_MASK(MANAGEMENT_METHOD_SET);
    if (method_mask == get_set)
    {
        return "get, set";
    }
    if (method_mask == MANAGEMENT_METHOD_MASK(MANAGEMENT_METHOD_GET))
    {
        return "get";
    }
    if (method_mask == MANAGEMENT_METHOD_MASK(MANAGEMENT_METHOD_ACTION))
    {
        return "action";
    }
    return "unknown";
}

static void emit_lines(const char* text)
{
    const char* cursor = text;
    while (*cursor != '\0')
    {
        const char* end = strstr(cursor, "\r\n");
        if (end == NULL)
        {
            LOG_PLAIN("%s", cursor);
            return;
        }
        LOG_PLAIN("%.*s\r\n", (int)(end - cursor), cursor);
        cursor = end + 2;
    }
}

static void emit_discovery(const management_request_t* request)
{
    if (request->value.kind != MANAGEMENT_VALUE_TEXT)
    {
        emit_lines(management_cli_help_overview());
        return;
    }

    const management_resource_descriptor_t* descriptor =
        management_catalog_find(request->value.data.text);
    if (descriptor == NULL)
    {
        return;
    }
    LOG_PLAIN("%s\r\n", descriptor->path);
    LOG_PLAIN("  %s\r\n", descriptor->help);
    LOG_PLAIN("  Access: %s\r\n", help_access(descriptor->method_mask));
    LOG_PLAIN("  Usage:\r\n%s", management_cli_help_usage(descriptor->resource));
}

static void emit_net(const management_runtime_view_t* view)
{
    LOG_PLAIN("NETWORK\r\n");
    LOG_PLAIN("  Local address : %u.%u.%u.%u/%u\r\n",
              view->local_ip.address[0], view->local_ip.address[1],
              view->local_ip.address[2], view->local_ip.address[3],
              view->local_ip.prefix_length);
    LOG_PLAIN("  Remote address: %u.%u.%u.%u\r\n", view->remote_ip[0],
              view->remote_ip[1], view->remote_ip[2], view->remote_ip[3]);
    LOG_PLAIN("  Gateway       : %u.%u.%u.%u\r\n", view->gateway[0],
              view->gateway[1], view->gateway[2], view->gateway[3]);
    LOG_PLAIN("  UDP ports     : local=%u  remote=%u\r\n",
              (unsigned int)view->local_udp_port,
              (unsigned int)view->remote_udp_port);
}

static void emit_config(const management_runtime_view_t* view)
{
    LOG_PLAIN("CONFIGURATION\r\n");
    LOG_PLAIN("  Mode          : %s\r\n",
              operating_mode_name(view->operating_mode));
    LOG_PLAIN("  Log level     : %s\r\n", loglevel_name(view->log_level));
    emit_net(view);
    LOG_PLAIN("V.24\r\n");
    LOG_PLAIN("  Baudrate      : %" PRIu32 "\r\n", view->v24_baudrate);
    LOG_PLAIN("  TX clock      : %s\r\n",
              view->v24_external_clock ? "external" : "internal");
    emit_inverted_signals("  Inverted signals: ", view->v24_invert_mask);
    LOG_PLAIN("  CTS restart   : %s\r\n",
              view->v24_cts_restart ? "on" : "off");
}

static void emit_status(const management_runtime_view_t* view)
{
    const management_status_snapshot_t* status = &view->status;
    const payload_statistics_t* stats = &status->stats;

    LOG_PLAIN("status: ok\r\n");
    LOG_PLAIN("Operating mode: %s\r\n",
              operating_mode_name(view->operating_mode));
    if (view->operating_mode == OPERATING_MODE_GPIO)
    {
        LOG_PLAIN("Bridge data path: disabled (manual GPIO mode)\r\n");
        return;
    }

    LOG_PLAIN("Current Baudrate estimation on pin %u: %.1f Hz\r\n",
              V24_RXC, status->rck_hz);
    if (status->xck_available)
    {
        LOG_PLAIN("Current Baudrate estimation on pin %u: %.1f Hz\r\n",
                  V24_TXC_DCE, status->xck_hz);
    }
    LOG_PLAIN("Current Baudrate estimation based on DMA: %" PRIu64
              " Hz\r\n", status->rates.dma_baudrate_hz);

    LOG_PLAIN("PIPE STATS\r\n");
    LOG_PLAIN("  Traffic\r\n");
    LOG_PLAIN("    Frames    : udp_rx=%" PRIu64 "  hdlc_tx=%" PRIu64
              "  hdlc_rx=%" PRIu64 "\r\n",
              stats->udp_rx_frames, stats->hdlc_tx_frames,
              stats->hdlc_frame_ready);
    LOG_PLAIN("    UDP TX    : attempts=%" PRIu64 "  ok=%" PRIu64
              "  fail=%" PRIu64 "\r\n",
              stats->udp_tx_attempt_frames, stats->udp_tx_success_frames,
              stats->udp_tx_failed_frames);
    LOG_PLAIN("    Backlog   : tx->ready_gap=%" PRIu64
              "  ready->udp_gap=%" PRIu64 "\r\n",
              status->tx_ready_gap, status->ready_udp_gap);
    LOG_PLAIN("    Serial    : rx_bytes=%" PRIu64 "  tx_bytes=%" PRIu64
              "\r\n", stats->serial_rx_bytes, status->tx_wire_bytes);
    LOG_PLAIN("    Rates     : udp_rx=%" PRIu64 "/s  hdlc_rx=%" PRIu64
              "/s  udp_tx=%" PRIu64 "/s  fail=%" PRIu64
              "/s  rx_bytes=%" PRIu64 "/s\r\n",
              status->rates.udp_rx_rate_fps,
              status->rates.hdlc_rx_rate_fps,
              status->rates.udp_tx_rate_fps,
              status->rates.decode_fail_rate,
              status->rates.serial_rx_rate_bps);

    LOG_PLAIN("  Decode / Sync\r\n");
    LOG_PLAIN("    Decode    : frame_ready=%" PRIu64 "  ok=%" PRIu64
              "  fail=%" PRIu64 "\r\n",
              stats->hdlc_frame_ready, stats->hdlc_decode_ok,
              stats->hdlc_decode_fail);
    LOG_PLAIN("    FailReason: invalid=%" PRIu64 "  short=%" PRIu64
              "  long=%" PRIu64 "  unstuff=%" PRIu64 "  crc=%" PRIu64
              "\r\n", stats->decode_fail_invalid_frame,
              stats->decode_fail_too_short,
              stats->decode_fail_payload_too_long,
              stats->decode_fail_unstuff_error,
              stats->decode_fail_crc_mismatch);

    const hdlc_decode_failure_diagnostic_t* failure =
        &status->first_decode_failure;
    if (!failure->valid)
    {
        LOG_PLAIN("    FirstFail : none\r\n");
    }
    else
    {
        char prefix[(HDLC_DECODE_FAILURE_PREFIX_SIZE * 2U) + 1U] = {0};
        for (size_t index = 0U; index < failure->prefix_length; ++index)
        {
            (void)snprintf(&prefix[index * 2U], 3U, "%02X",
                           failure->prefix[index]);
        }
        LOG_PLAIN("    FirstFail : reason=%s  encoded=%zu  decoded=%zu"
                  "  raw_seen=%zu\r\n",
                  decode_failure_reason_name(failure->reason),
                  failure->encoded_length, failure->decoded_length,
                  failure->raw_bit_index);
        LOG_PLAIN("                off=%u  align=%s  rx_crc=%04" PRIX16
                  "  calc_crc=%04" PRIX16 "\r\n",
                  failure->sync_bit_offset,
                  failure->align_shift_right ? "right" : "left",
                  failure->received_crc, failure->calculated_crc);
        LOG_PLAIN("                prefix=%s\r\n", prefix);
    }
    LOG_PLAIN("    SyncState : %s\r\n",
              sync_state_name(status->sync_state));
    LOG_PLAIN("    SyncWait  : syncing=%" PRIu64 "  synced=%" PRIu64
              "\r\n", stats->sync_lookahead_wait_syncing,
              stats->sync_lookahead_wait_synced);
    LOG_PLAIN("    SyncMaint : consume=%" PRIu64
              "  hardcap_events=%" PRIu64 "  hardcap_bytes=%" PRIu64
              "\r\n", stats->sync_candidate_consume,
              stats->sync_hardcap_drop_events,
              stats->sync_hardcap_drop_bytes);
    LOG_PLAIN("    Resync    : idle=%" PRIu64 "  hard=%" PRIu64
              "  no_progress=%" PRIu64 "\r\n",
              stats->resync_idle_timeout_count,
              stats->resync_hard_fail_count,
              stats->resync_no_progress_count);
    LOG_PLAIN("    Accum     : pos=%zu  proc=%zu  state=%d  off=%u"
              "  cand_valid=%d  cand_end=%zu\r\n",
              status->accumulator_position,
              status->accumulator_processed, status->sync_state,
              status->accumulator_bit_offset,
              status->accumulator_candidate_valid ? 1 : 0,
              status->accumulator_candidate_end);
    LOG_PLAIN("    RX Health : acc_pos_max=%" PRIu64
              "  rx_fifo_stall=%" PRIu64 "  rx_drop_acc_full=%" PRIu64
              "\r\n", stats->accumulator_pos_max,
              stats->rx_fifo_stall_events,
              stats->serial_rx_drop_acc_full);
    LOG_PLAIN("               hunt_idle_drop=%" PRIu64
              "  dma_ovf_words=%" PRIu32 "\r\n",
              stats->hunt_idle_drop_bytes, status->dma_overflow_words);

    LOG_PLAIN("  Buffers\r\n");
    LOG_PLAIN("    TX Queue  : used=%zu/%zu  active=%d\r\n",
              status->tx_queue_used, status->tx_queue_capacity,
              status->tx_frame_active ? 1 : 0);
    LOG_PLAIN("    HighWater : tx=%" PRIu64 "  event=%" PRIu64
              "  log=%" PRIu64 "\r\n", stats->tx_queue_used_max,
              stats->event_queue_used_max, stats->log_queue_used_max);
    LOG_PLAIN("    W5500 Buf : rx_no_room_events=%" PRIu64
              "  tx_full_events=%" PRIu64 "\r\n",
              stats->udp_rx_buffer_full_counts,
              stats->udp_tx_buffer_full_counts);
    LOG_PLAIN("    Drops     : tx=%" PRIu64 "  event=%" PRIu64
              "  log=%" PRIu64 "\r\n", stats->tx_queue_drop_frames,
              stats->event_queue_drop_events, stats->log_drop_lines);
    LOG_PLAIN("    Recons    : len=%zu\r\n",
              status->reconstructed_length);
    LOG_PLAIN("    Throttle  : udp_rx_enter=%" PRIu64
              "  udp_rx_skips=%" PRIu64 "\r\n",
              stats->udp_rx_throttle_enter,
              stats->udp_rx_throttle_skips);
    LOG_PLAIN("    PIO TX    : stalled=%u  underruns=%" PRIu32
              "  cts_restarts=%" PRIu32 "\r\n",
              status->pio_stalled ? 1U : 0U,
              status->tx_fifo_underruns, status->tx_cts_restarts);
    LOG_PLAIN("    CTS       : level=%u  edges=%" PRIu32
              "  restart=%u\r\n", status->cts_level ? 1U : 0U,
              status->cts_edge_count,
              status->cts_restart_enabled ? 1U : 0U);
}

static void emit_memory(const management_memory_snapshot_t* memory)
{
    LOG_PLAIN("=== Memory Usage ===\r\n");
    LOG_PLAIN("Static RAM     : %" PRIu32 " bytes\r\n",
              memory->ram_static_used);
    LOG_PLAIN("Heap allocated : %" PRIu32 " bytes\r\n",
              memory->heap_allocated);
    LOG_PLAIN("RAM total/free : %" PRIu32 " / %" PRIu32 " bytes\r\n",
              memory->ram_total, memory->ram_free);
    LOG_PLAIN("Flash used     : %" PRIu32 " bytes\r\n",
              memory->flash_used);
    LOG_PLAIN("Flash total/free: %" PRIu32 " / %" PRIu32 " bytes\r\n",
              memory->flash_total, memory->flash_free);
}

static void emit_pins(const management_request_t* request)
{
    if (request->resource == MANAGEMENT_RESOURCE_PIN &&
        request->value.kind == MANAGEMENT_VALUE_GPIO)
    {
        const gpio_signal_info_t* signal =
            gpio_control_signal_at((size_t)request->value.data.gpio.signal);
        if (signal != NULL)
        {
            LOG_PLAIN("Pin %s (GPIO %u)\r\n", signal->name,
                      signal->gpio_num);
            LOG_PLAIN("  Direction: %s\r\n",
                      signal->direction == GPIO_SIGNAL_OUTPUT
                          ? "OUTPUT" : "INPUT");
            LOG_PLAIN("  Manual control: %s\r\n",
                      signal->reserved ? "reserved"
                                       : (signal->direction == GPIO_SIGNAL_OUTPUT
                                              ? "GPIO mode only" : "read-only"));
        }
        return;
    }

    LOG_PLAIN("PINS\r\n");
    LOG_PLAIN("  Signal      GPIO  Direction  Manual control\r\n");
    for (size_t i = 0U; i < gpio_control_signal_count(); i++)
    {
        const gpio_signal_info_t* signal = gpio_control_signal_at(i);
        if (signal != NULL)
        {
            LOG_PLAIN("  %-10s  %-4u  %-9s  %s\r\n", signal->name,
                      signal->gpio_num,
                      signal->direction == GPIO_SIGNAL_OUTPUT
                          ? "output" : "input",
                      signal->reserved
                          ? "reserved"
                          : (signal->direction == GPIO_SIGNAL_OUTPUT
                                 ? "GPIO mode" : "read-only"));
        }
    }
}

static void emit_payload(const management_request_t* request,
                         const management_response_t* response,
                         const char* canonical_path)
{
    switch (request->resource)
    {
    case MANAGEMENT_RESOURCE_DISCOVERY:
        emit_discovery(request);
        break;
    case MANAGEMENT_RESOURCE_STATUS:
        emit_status(&response->snapshot);
        break;
    case MANAGEMENT_RESOURCE_MEMORY:
        emit_memory(&response->snapshot.memory);
        break;
    case MANAGEMENT_RESOURCE_CONFIG:
        emit_config(&response->snapshot);
        break;
    case MANAGEMENT_RESOURCE_NET:
        emit_net(&response->snapshot);
        break;
    case MANAGEMENT_RESOURCE_V24:
        LOG_PLAIN("V.24\r\n");
        LOG_PLAIN("  Baudrate    : %" PRIu32 "\r\n",
                  response->snapshot.v24_baudrate);
        LOG_PLAIN("  TX clock    : %s\r\n",
                  response->snapshot.v24_external_clock
                      ? "external" : "internal");
        emit_inverted_signals("  Inverted signals: ",
                              response->snapshot.v24_invert_mask);
        LOG_PLAIN("  CTS restart : %s\r\n",
                  response->snapshot.v24_cts_restart ? "on" : "off");
        break;
    case MANAGEMENT_RESOURCE_PINS:
    case MANAGEMENT_RESOURCE_PIN:
        emit_pins(request);
        break;
    case MANAGEMENT_RESOURCE_SYSTEM_LOGLEVEL:
        LOG_PLAIN("Log level: %s\r\n",
                  loglevel_name(response->value.data.u32));
        break;
    case MANAGEMENT_RESOURCE_ACTION_RESET_STATS:
        LOG_PLAIN("Statistics reset.\r\n");
        break;
    default:
        emit_scalar(canonical_path, &response->value);
        break;
    }
}

void management_serial_handle_line(app_ctx_t* app, const char* line)
{
    static uint32_t sequence = 0U;
    if (app == NULL || line == NULL || line[0] == '\0')
    {
        return;
    }

    const bool framed = line[0] == '@';
    const char* command = framed ? line + 1 : line;
    while (*command == ' ')
    {
        command++;
    }
    if (framed)
    {
        sequence++;
    }

    management_request_t request;
    management_response_t response;
    const char* canonical_path = "";
    const management_result_t parse_result =
        management_cli_parse_human(command, &request, &canonical_path);
    if (parse_result == MANAGEMENT_RESULT_OK)
    {
        (void)management_app_execute(app, &request, &response);
    }
    else
    {
        memset(&request, 0, sizeof(request));
        memset(&response, 0, sizeof(response));
        response.result = parse_result;
    }

    if (response.result != MANAGEMENT_RESULT_OK &&
        response.result != MANAGEMENT_RESULT_ACCEPTED)
    {
        if (framed)
        {
            LOG_PLAIN("@response %" PRIu32 " end error %s\r\n", sequence,
                      management_result_name(response.result));
        }
        else
        {
            LOG_PLAIN("error: %s (try 'help')\r\n",
                      management_result_name(response.result));
        }
        return;
    }

    if (framed)
    {
        LOG_PLAIN("@response %" PRIu32 " begin %s\r\n", sequence,
                  canonical_path);
    }
    emit_payload(&request, &response, canonical_path);
    if (framed)
    {
        LOG_PLAIN("@response %" PRIu32 " end %s\r\n", sequence,
                  management_result_name(response.result));
    }

    if (!management_app_apply_effect(app, &response.effect))
    {
        LOG_ERROR("management effect failed\r\n");
    }
}
