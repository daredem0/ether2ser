#ifndef RUNTIME_METRICS_H
#define RUNTIME_METRICS_H

#include <stdbool.h>
#include <stdint.h>

#define RUNTIME_METRICS_INTERVAL_MS 1000U

typedef struct
{
    uint64_t udp_rx_frames;
    uint64_t hdlc_rx_frames;
    uint64_t udp_tx_frames;
    uint64_t decode_failures;
    uint64_t serial_rx_bytes;
} runtime_metric_counters_t;

typedef struct
{
    uint64_t udp_rx_rate_fps;
    uint64_t hdlc_rx_rate_fps;
    uint64_t udp_tx_rate_fps;
    uint64_t decode_fail_rate;
    uint64_t serial_rx_rate_bps;
    uint64_t dma_baudrate_hz;
} runtime_metric_rates_t;

typedef struct
{
    runtime_metric_counters_t previous;
    runtime_metric_rates_t    rates;
    uint32_t                  previous_ms;
    bool                      initialized;
} runtime_metrics_t;

void runtime_metrics_init(runtime_metrics_t* metrics);
bool runtime_metrics_update(runtime_metrics_t* metrics, uint32_t now_ms,
                            const runtime_metric_counters_t* counters);
void runtime_metrics_snapshot(const runtime_metrics_t* metrics, runtime_metric_rates_t* rates);

#endif /* RUNTIME_METRICS_H */
