#include "system/management_app.h"

#include <string.h>

#include "drivers/gpio_control.h"
#include "drivers/gpio_driver.h"
#include "drivers/mac_address.h"
#include "drivers/pio_tx_rx_driver.h"
#include "drivers/tx_queue.h"
#include "drivers/w5500_driver.h"
#include "system/common.h"
#include "system/event_queue.h"

static uint8_t mask_prefix_length(const uint8_t mask[4])
{
    uint8_t prefix = 0U;
    bool zero_seen = false;
    for (size_t byte = 0U; byte < 4U; byte++)
    {
        for (uint8_t bit = 0U; bit < 8U; bit++)
        {
            const bool set = (mask[byte] & (uint8_t)(0x80U >> bit)) != 0U;
            if (set && !zero_seen)
            {
                prefix++;
            }
            else
            {
                zero_seen = true;
            }
        }
    }
    return prefix;
}

static void prefix_to_mask(uint8_t prefix, uint8_t mask[4])
{
    for (size_t i = 0U; i < 4U; i++)
    {
        if (prefix >= 8U)
        {
            mask[i] = UINT8_MAX;
            prefix -= 8U;
        }
        else if (prefix == 0U)
        {
            mask[i] = 0U;
        }
        else
        {
            mask[i] = (uint8_t)(UINT8_MAX << (8U - prefix));
            prefix = 0U;
        }
    }
}

static uint32_t polarities_to_mask(const V24_POLARITIES_T* polarities)
{
    uint32_t mask = 0U;
    mask |= polarities->tx_polarities.txd_inverted ? (1UL << 0U) : 0U;
    mask |= polarities->tx_polarities.txc_inverted ? (1UL << 1U) : 0U;
    mask |= polarities->tx_polarities.cts_inverted ? (1UL << 2U) : 0U;
    mask |= polarities->tx_polarities.rts_inverted ? (1UL << 3U) : 0U;
    mask |= polarities->tx_polarities.dtr_inverted ? (1UL << 4U) : 0U;
    mask |= polarities->rx_polarities.rxd_inverted ? (1UL << 5U) : 0U;
    mask |= polarities->rx_polarities.rxc_inverted ? (1UL << 6U) : 0U;
    mask |= polarities->rx_polarities.dcd_inverted ? (1UL << 7U) : 0U;
    return mask;
}

static V24_POLARITIES_T mask_to_polarities(uint32_t mask)
{
    return (V24_POLARITIES_T){
        .tx_polarities = {
            .txd_inverted = (mask & (1UL << 0U)) != 0U,
            .txc_inverted = (mask & (1UL << 1U)) != 0U,
            .cts_inverted = (mask & (1UL << 2U)) != 0U,
            .rts_inverted = (mask & (1UL << 3U)) != 0U,
            .dtr_inverted = (mask & (1UL << 4U)) != 0U,
        },
        .rx_polarities = {
            .rxd_inverted = (mask & (1UL << 5U)) != 0U,
            .rxc_inverted = (mask & (1UL << 6U)) != 0U,
            .dcd_inverted = (mask & (1UL << 7U)) != 0U,
        },
    };
}

void management_app_capture(app_ctx_t* app, management_runtime_view_t* runtime)
{
    memset(runtime, 0, sizeof(*runtime));
    runtime->operating_mode = app->operating_mode;
    runtime->frame_active   = tx_queue_has_active_frame(&app->tx_queue);
    runtime->queue_empty    = tx_queue_is_empty(&app->tx_queue);

    for (size_t i = 0U; i < gpio_control_signal_count(); i++)
    {
        const gpio_signal_info_t* signal = gpio_control_signal_at(i);
        if (signal != NULL && gpio_get(signal->gpio_num) != 0)
        {
            runtime->gpio_levels |= 1UL << (uint32_t)signal->signal;
        }
    }

    wiz_NetInfo net_info;
    wizchip_getnetinfo(&net_info);
    memcpy(runtime->local_ip.address, net_info.ip, sizeof(runtime->local_ip.address));
    runtime->local_ip.prefix_length = mask_prefix_length(net_info.sn);
    memcpy(runtime->gateway, net_info.gw, sizeof(runtime->gateway));
    memcpy(runtime->remote_ip, app->destination_config.ip_address, sizeof(runtime->remote_ip));
    runtime->local_udp_port     = app->local_config.port;
    runtime->remote_udp_port    = app->destination_config.port;
    runtime->v24_baudrate       = (uint32_t)app->v24_config.baudrate;
    runtime->v24_invert_mask    = polarities_to_mask(&app->v24_config.polarities);
    runtime->v24_external_clock = app->v24_config.external_clock;
    runtime->v24_cts_restart    = v24_tx_get_cts_restart_enabled();
    runtime->log_level          = (uint32_t)get_loglevel();
    memory_status_snapshot(&runtime->memory);

    management_status_snapshot_t* status = &runtime->status;
    status->stats = app->stats;
    runtime_metrics_snapshot(&app->runtime_metrics, &status->rates);
    if (app->stats.hdlc_tx_frames > app->stats.hdlc_frame_ready)
    {
        status->tx_ready_gap = app->stats.hdlc_tx_frames - app->stats.hdlc_frame_ready;
    }
    if (app->stats.hdlc_frame_ready > app->stats.udp_tx_success_frames)
    {
        status->ready_udp_gap =
            app->stats.hdlc_frame_ready - app->stats.udp_tx_success_frames;
    }
    status->tx_wire_bytes                 = app->tx_queue.tx_wire_bytes;
    status->sync_state                    = (int)app->accumulator.state;
    status->accumulator_position          = app->accumulator.position;
    status->accumulator_processed         = app->accumulator.processed;
    status->accumulator_bit_offset        = app->accumulator.bit_offset;
    status->accumulator_candidate_valid   = app->accumulator.candidate_valid;
    status->accumulator_candidate_end     = app->accumulator.candidate_end;
    status->reconstructed_length          = app->reconstructed_frame.length;
    status->tx_queue_used                 = app->tx_queue.queue_buffer.count;
    status->tx_queue_capacity             = app->tx_queue.queue_buffer.capacity;
    status->tx_frame_active               = runtime->frame_active;
    hdlc_decode_failure_snapshot(&status->first_decode_failure);

    if (app->operating_mode == OPERATING_MODE_BRIDGE)
    {
        status->rck_hz = baudrate_estimator_get_current_estimation(V24_RXC);
        status->xck_available = app->v24_config.external_clock;
        if (status->xck_available)
        {
            status->xck_hz = baudrate_estimator_get_current_estimation(V24_TXC_DCE);
        }
        status->dma_overflow_words = rx_dma_get_overflow_words();
        v24_tx_status_t v24_status;
        v24_tx_status_snapshot(&v24_status);
        status->pio_stalled         = v24_status.pio_stalled;
        status->tx_fifo_underruns   = v24_status.tx_fifo_underruns;
        status->tx_cts_restarts     = v24_status.tx_cts_restarts;
        status->cts_level           = v24_status.cts_level;
        status->cts_edge_count      = v24_status.cts_edge_count;
        status->cts_restart_enabled = v24_status.cts_restart_enabled;
    }
}

management_result_t management_app_execute(app_ctx_t* app,
                                           const management_request_t* request,
                                           management_response_t* response)
{
    management_runtime_view_t runtime;
    management_app_capture(app, &runtime);
    return management_execute(request, &runtime, response);
}

static bool push_simple_event(event_type_t type)
{
    const event_t event = {.type = type, .data.ptr = NULL, .data_len = 0U, .is_inline = false};
    return event_queue_push(&event);
}

static bool apply_set_effect(app_ctx_t* app, const management_effect_t* effect)
{
    switch (effect->resource)
    {
    case MANAGEMENT_RESOURCE_SYSTEM_MODE:
        app->operating_mode = effect->value.data.mode;
        break;
    case MANAGEMENT_RESOURCE_GPIO:
        return gpio_driver_set_manual_output(app->operating_mode,
                                             effect->value.data.gpio.signal,
                                             effect->value.data.gpio.level);
    case MANAGEMENT_RESOURCE_NET_IP_LOCAL:
    {
        wiz_NetInfo info;
        wizchip_getnetinfo(&info);
        memcpy(info.ip, effect->value.data.ipv4_cidr.address, sizeof(info.ip));
        prefix_to_mask(effect->value.data.ipv4_cidr.prefix_length, info.sn);
        mac_address_from_ipv4(info.ip, info.mac);
        wizchip_setnetinfo(&info);
        memcpy(app->local_config.ip_address, info.ip, sizeof(app->local_config.ip_address));
        break;
    }
    case MANAGEMENT_RESOURCE_NET_IP_REMOTE:
        memcpy(app->destination_config.ip_address, effect->value.data.ipv4,
               sizeof(app->destination_config.ip_address));
        break;
    case MANAGEMENT_RESOURCE_NET_IP_GATEWAY:
    {
        wiz_NetInfo info;
        wizchip_getnetinfo(&info);
        memcpy(info.gw, effect->value.data.ipv4, sizeof(info.gw));
        wizchip_setnetinfo(&info);
        break;
    }
    case MANAGEMENT_RESOURCE_NET_UDP_PORT_LOCAL:
        app->local_config.port = effect->value.data.u16;
        if (app->operating_mode == OPERATING_MODE_BRIDGE &&
            w5500_reconfigure_udp_socket(&app->local_config) != E2S_OK)
        {
            return false;
        }
        break;
    case MANAGEMENT_RESOURCE_NET_UDP_PORT_REMOTE:
        app->destination_config.port = effect->value.data.u16;
        break;
    case MANAGEMENT_RESOURCE_V24_BAUDRATE:
        reinit_v24_config(&app->v24_config, (V24_BAUDRATE_T)effect->value.data.u32);
        if (app->operating_mode == OPERATING_MODE_BRIDGE)
        {
            tx_clock_update_settings(&app->v24_config);
        }
        break;
    case MANAGEMENT_RESOURCE_V24_TXCLOCK:
        app->v24_config.external_clock = effect->value.data.boolean;
        break;
    case MANAGEMENT_RESOURCE_V24_INVERT:
        app->v24_config.polarities = mask_to_polarities(effect->value.data.u32);
        if (app->operating_mode == OPERATING_MODE_BRIDGE)
        {
            tx_clock_update_settings(&app->v24_config);
            rx_clock_update_settings(&app->v24_config.polarities.rx_polarities);
        }
        break;
    case MANAGEMENT_RESOURCE_V24_CTS_RESTART:
        v24_tx_set_cts_restart_enabled(effect->value.data.boolean);
        break;
    case MANAGEMENT_RESOURCE_SYSTEM_LOGLEVEL:
        set_loglevel((log_level_t)effect->value.data.u32);
        break;
    default:
        return false;
    }
    return true;
}

bool management_app_apply_effect(app_ctx_t* app, const management_effect_t* effect)
{
    if (app == NULL || effect == NULL)
    {
        return false;
    }

    bool applied = true;
    switch (effect->kind)
    {
    case MANAGEMENT_EFFECT_NONE:
        return true;
    case MANAGEMENT_EFFECT_SET:
        applied = apply_set_effect(app, effect);
        break;
    case MANAGEMENT_EFFECT_SAVE:
        return push_simple_event(EV_SAVE_CONFIG);
    case MANAGEMENT_EFFECT_WIPE:
        return push_simple_event(EV_WIPE_CONFIG);
    case MANAGEMENT_EFFECT_REBOOT:
        return push_simple_event(EV_REBOOT);
    default:
        return false;
    }
    if (!applied)
    {
        return false;
    }
    if (effect->persist && !push_simple_event(EV_SAVE_CONFIG))
    {
        return false;
    }
    if (effect->reboot && !push_simple_event(EV_REBOOT))
    {
        return false;
    }
    return true;
}
