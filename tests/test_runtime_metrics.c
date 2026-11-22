#include <string.h>

#include "system/runtime_metrics.h"
#include "unity.h"

void test_runtime_metrics_first_sample_establishes_baseline(void)
{
    runtime_metrics_t metrics;
    runtime_metrics_init(&metrics);
    const runtime_metric_counters_t counters = {.udp_rx_frames = 10U, .serial_rx_bytes = 20U};

    TEST_ASSERT_FALSE(runtime_metrics_update(&metrics, 100U, &counters));
    runtime_metric_rates_t rates;
    runtime_metrics_snapshot(&metrics, &rates);
    const runtime_metric_rates_t zero = {0};
    TEST_ASSERT_EQUAL_MEMORY(&zero, &rates, sizeof(rates));
}

void test_runtime_metrics_updates_after_fixed_interval(void)
{
    runtime_metrics_t metrics;
    runtime_metrics_init(&metrics);
    const runtime_metric_counters_t baseline = {0};
    TEST_ASSERT_FALSE(runtime_metrics_update(&metrics, 100U, &baseline));

    const runtime_metric_counters_t counters = {
        .udp_rx_frames   = 4U,
        .hdlc_rx_frames  = 3U,
        .udp_tx_frames   = 2U,
        .decode_failures = 1U,
        .serial_rx_bytes = 2000U,
    };
    TEST_ASSERT_TRUE(runtime_metrics_update(&metrics, 1100U, &counters));

    runtime_metric_rates_t rates;
    runtime_metrics_snapshot(&metrics, &rates);
    TEST_ASSERT_EQUAL_UINT64(4U, rates.udp_rx_rate_fps);
    TEST_ASSERT_EQUAL_UINT64(3U, rates.hdlc_rx_rate_fps);
    TEST_ASSERT_EQUAL_UINT64(2U, rates.udp_tx_rate_fps);
    TEST_ASSERT_EQUAL_UINT64(1U, rates.decode_fail_rate);
    TEST_ASSERT_EQUAL_UINT64(2000U, rates.serial_rx_rate_bps);
    TEST_ASSERT_EQUAL_UINT64(16000U, rates.dma_baudrate_hz);
}

void test_runtime_metrics_snapshot_is_observational(void)
{
    runtime_metrics_t metrics;
    runtime_metrics_init(&metrics);
    const runtime_metric_counters_t baseline = {0};
    const runtime_metric_counters_t counters = {.udp_rx_frames = 7U};
    (void)runtime_metrics_update(&metrics, 10U, &baseline);
    (void)runtime_metrics_update(&metrics, 1010U, &counters);

    runtime_metric_rates_t first;
    runtime_metric_rates_t second;
    runtime_metrics_snapshot(&metrics, &first);
    runtime_metrics_snapshot(&metrics, &second);
    TEST_ASSERT_EQUAL_MEMORY(&first, &second, sizeof(first));
    TEST_ASSERT_EQUAL_UINT64(7U, second.udp_rx_rate_fps);
}

void test_runtime_metrics_ignores_subinterval_polling(void)
{
    runtime_metrics_t metrics;
    runtime_metrics_init(&metrics);
    const runtime_metric_counters_t baseline = {0};
    const runtime_metric_counters_t early = {.udp_rx_frames = 99U};
    (void)runtime_metrics_update(&metrics, 50U, &baseline);

    TEST_ASSERT_FALSE(runtime_metrics_update(&metrics, 1049U, &early));
    runtime_metric_rates_t rates;
    runtime_metrics_snapshot(&metrics, &rates);
    TEST_ASSERT_EQUAL_UINT64(0U, rates.udp_rx_rate_fps);
}
