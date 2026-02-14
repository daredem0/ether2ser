/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/baudrate_monitor.c
 * Purpose: RXC edge-counting baudrate estimator implementation.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "baudrate_monitor.h"

// Standard library headers
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Library Headers
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/timer.h"
#include "pico/time.h"
#include "pico/types.h"

// Project Headers
#include "platform/pinmap.h"
#include "system/common.h"

// Generated headers

#define PIN_COUNT 25
// Mark estimate stale if no edges for this long (sporadic clocks).
#define BAUD_STALE_US 50000U
// Short sampling period to quickly react to bursts.
#define BAUD_TIMER_MS 20
// EMA smoothing for burst-to-burst stability.
#define BAUD_EMA_ALPHA 0.2F
static volatile uint32_t edge_count[PIN_COUNT] = {0};
static repeating_timer_t baud_timer;
static volatile bool     baud_ready[PIN_COUNT]    = {false};
static volatile float    baud_hz[PIN_COUNT]       = {0.0F};
static size_t            current_pin_count        = 0;
static volatile uint8_t  monitored_pin[PIN_COUNT] = {0};
// First/last edge timestamps within the current sample window.
static volatile uint64_t first_edge_time_us[PIN_COUNT] = {0};
static volatile uint64_t last_edge_time_us[PIN_COUNT]  = {0};

static void rxc_edge_isr(uint gpio, uint32_t events)
{
    // (void)gpio;
    (void)events;
    // Record the first edge in the current window, then update last edge time.
    uint64_t now_us = time_us_64();
    if (edge_count[gpio] == 0)
    {
        first_edge_time_us[gpio] = now_us;
    }
    last_edge_time_us[gpio] = now_us;
    edge_count[gpio]++;
}

float baudrate_estimator_get_current_estimation(V24_PIN_T pin)
{
    baud_ready[pin] = false;
    return baud_hz[pin];
}

static bool baud_timer_cb(repeating_timer_t* timer)
{
    (void)timer;

    uint64_t now_us = time_us_64();
    for (size_t pin_index = 0; pin_index < current_pin_count; ++pin_index)
    {
        uint32_t edges =
            __atomic_exchange_n(&edge_count[monitored_pin[pin_index]], 0, __ATOMIC_RELAXED);
        if (edges > 1)
        {
            // Use first/last edge timestamps to avoid timer jitter skewing Hz.
            uint32_t save     = save_and_disable_interrupts();
            uint64_t first_us = first_edge_time_us[monitored_pin[pin_index]];
            uint64_t last_us  = last_edge_time_us[monitored_pin[pin_index]];
            restore_interrupts(save);
            if (last_us > first_us)
            {
                uint64_t elapsed_us = last_us - first_us;
                float    inst_hz = ((float)(edges - 1) * (float)US_PER_SECOND) / (float)elapsed_us;
                if (baud_hz[monitored_pin[pin_index]] <= 0.0F)
                {
                    baud_hz[monitored_pin[pin_index]] = inst_hz;
                }
                else
                {
                    // Smooth bursts with a simple EMA.
                    baud_hz[monitored_pin[pin_index]] =
                        (BAUD_EMA_ALPHA * inst_hz) +
                        ((1.0F - BAUD_EMA_ALPHA) * baud_hz[monitored_pin[pin_index]]);
                }
                baud_ready[monitored_pin[pin_index]] = true;
            }
        }
        else
        {
            // If no edges recently, mark estimate as stale without forcing zero.
            if (now_us - last_edge_time_us[monitored_pin[pin_index]] > BAUD_STALE_US)
            {
                baud_ready[monitored_pin[pin_index]] = false;
            }
        }
    }

    return true;
}

void baudrate_estimator_init(V24_PIN_T pin)
{
    assert(pin <= PIN_COUNT);
    static bool initialized = false;
    if (!initialized)
    {
        gpio_set_irq_enabled_with_callback(pin, GPIO_IRQ_EDGE_RISE, true, &rxc_edge_isr);
        add_repeating_timer_ms(BAUD_TIMER_MS, baud_timer_cb, NULL, &baud_timer);
        initialized = true;
    }
    else
    {
        gpio_set_irq_enabled(pin, GPIO_IRQ_EDGE_RISE, true);
    }
    if (current_pin_count < PIN_COUNT)
    {
        monitored_pin[current_pin_count++] = pin;
    }
}
