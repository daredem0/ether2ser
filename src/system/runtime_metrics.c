#include "system/runtime_metrics.h"

#include <string.h>

static uint64_t monotonic_delta(uint64_t current, uint64_t previous)
{
    return current >= previous ? current - previous : 0U;
}

void runtime_metrics_init(runtime_metrics_t* metrics)
{
    if (metrics != NULL)
    {
        memset(metrics, 0, sizeof(*metrics));
    }
}

bool runtime_metrics_update(runtime_metrics_t* metrics, uint32_t now_ms,
                            const runtime_metric_counters_t* counters)
{
    if (metrics == NULL || counters == NULL)
    {
        return false;
    }
    if (!metrics->initialized)
    {
        metrics->previous    = *counters;
        metrics->previous_ms = now_ms;
        metrics->initialized = true;
        return false;
    }

    const uint32_t elapsed_ms = now_ms - metrics->previous_ms;
    if (elapsed_ms < RUNTIME_METRICS_INTERVAL_MS)
    {
        return false;
    }

    const uint64_t udp_rx =
        monotonic_delta(counters->udp_rx_frames, metrics->previous.udp_rx_frames);
    const uint64_t hdlc_rx =
        monotonic_delta(counters->hdlc_rx_frames, metrics->previous.hdlc_rx_frames);
    const uint64_t udp_tx =
        monotonic_delta(counters->udp_tx_frames, metrics->previous.udp_tx_frames);
    const uint64_t decode_fail =
        monotonic_delta(counters->decode_failures, metrics->previous.decode_failures);
    const uint64_t serial_rx =
        monotonic_delta(counters->serial_rx_bytes, metrics->previous.serial_rx_bytes);

    metrics->rates.udp_rx_rate_fps    = (udp_rx * 1000U) / elapsed_ms;
    metrics->rates.hdlc_rx_rate_fps   = (hdlc_rx * 1000U) / elapsed_ms;
    metrics->rates.udp_tx_rate_fps    = (udp_tx * 1000U) / elapsed_ms;
    metrics->rates.decode_fail_rate   = (decode_fail * 1000U) / elapsed_ms;
    metrics->rates.serial_rx_rate_bps = (serial_rx * 1000U) / elapsed_ms;
    metrics->rates.dma_baudrate_hz    = (serial_rx * 8000U) / elapsed_ms;
    metrics->previous                 = *counters;
    metrics->previous_ms              = now_ms;
    return true;
}

void runtime_metrics_snapshot(const runtime_metrics_t* metrics, runtime_metric_rates_t* rates)
{
    if (rates == NULL)
    {
        return;
    }
    if (metrics == NULL)
    {
        memset(rates, 0, sizeof(*rates));
        return;
    }
    *rates = metrics->rates;
}
