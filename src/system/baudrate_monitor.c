




// Related headers
#include "baudrate_monitor.h"

// Standard library headers
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

// Library Headers
#include "pico/time.h"
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"
#include "hardware/gpio.h"

// Project Headers
#include "platform/pinmap.h"

// Generated headers

// #define US_PER_MS 1000
// #define MS_PER_SECOND 1000
// #define US_PER_SECOND (US_PER_MS * MS_PER_SECOND)
// #define SECONDS_PER_WINDOW 5
// #define US_WINDOW (SECONDS_PER_WINDOW * US_PER_SECOND)
#define PIN_COUNT 25
static volatile uint32_t edge_count[PIN_COUNT] = {0};
static repeating_timer_t baud_timer;
static volatile bool baud_ready[PIN_COUNT] = {false};
static volatile float baud_hz[PIN_COUNT] = {0.0f};
// static absolute_time_t window_start[PIN_COUNT] = {0};

static void rxc_edge_isr(uint gpio, uint32_t events){
    // (void)gpio;
    (void)events;
    edge_count[gpio]++;
}

float baudrate_estimator_get_current_estimation(V24_PIN_T pin){
    baud_ready[pin] = false;
    return baud_hz[pin];
}

static bool baud_timer_cb(repeating_timer_t *t){
    (void)t;

    uint32_t edges = __atomic_exchange_n(
        &edge_count[V24_RXC], 0, __ATOMIC_RELAXED);

    baud_hz[V24_RXC] = ((float)edges) / 5.0f;
    baud_ready[V24_RXC] = true;
    return true;
}


void baudrate_estimator_init(V24_PIN_T pin){
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_down(pin);
    gpio_set_irq_enabled_with_callback(
        pin, GPIO_IRQ_EDGE_RISE,
        true, &rxc_edge_isr);
    add_repeating_timer_ms(5000, baud_timer_cb, NULL, &baud_timer);

    // window_start[pin] = get_absolute_time();

}

// void baudrate_estimator_poll(V24_PIN_T pin){
//     absolute_time_t now = get_absolute_time();
//     if(absolute_time_diff_us(window_start[pin], now) > (int64_t)US_WINDOW){
//         uint32_t edges = __atomic_exchange_n(&edge_count[pin], 0, __ATOMIC_RELAXED);
//         window_start[pin] = delayed_by_us(window_start[pin], (int64_t)US_WINDOW);
//         float hz = ((float)edges * 0.5f) / SECONDS_PER_WINDOW;
//         printf("Baudrate approximated: %f Hz\r\n", hz);
//     }
// }
