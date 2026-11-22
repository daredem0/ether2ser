/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/drivers/pio_tx_rx_driver.c
 * Purpose: PIO TX/RX clock driver implementation.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "pio_tx_rx_driver.h"

// Standard library headers
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Library Headers
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/platform_defs.h"
#include "hardware/regs/pio.h"
#include "pico/time.h"
#include "pico/types.h"

// Project Headers
#include "drivers/gpio_driver.h"
#include "drivers/v24_config.h"
#include "drivers/v24_tx_state.h"
#include "platform/pinmap.h"
#include "system/common.h"

// Generated headers
#include "led_activity_mirror.pio.h"
#include "rck_rxd.pio.h"
#include "tck_txd.pio.h"
#include "xck_txd.pio.h"

/**
 * @brief Minimum RTS holdoff in microseconds used as a safety floor.
 */
#define V24_RTS_MIN_HOLDOFF 200U
/**
 * @brief RTS holdoff multiplier in bit-times.
 * The final holdoff is: tx_rts_holdoff_us = margin * t_bit_us.
 * This value was empirically determined to be the most stable for this system.
 */
#define V24_RTS_HOLDOFF_MARGIN 41U
#define RX_DMA_RING_BITS 12U
#define RX_DMA_RING_BYTES (1U << RX_DMA_RING_BITS)
#define RX_DMA_RING_WORDS (RX_DMA_RING_BYTES / sizeof(uint32_t))

typedef struct
{
    PIO               tx_pio;
    uint              tx_sm;
    PIO               rx_pio;
    uint              rx_sm;
    volatile uint32_t cts_toggle_seq;
    uint32_t          cts_statistics_baseline;
    volatile uint32_t rx_frame_start_seq;
    uint32_t          rx_frame_start_seen;
    uint32_t          tx_fifo_underrun_count;
    uint32_t          tx_cts_restart_count;
    int               rx_dma_chan;
    bool              rx_dma_enabled;
    uint32_t          rx_dma_words_consumed;
    uint32_t          rx_dma_overflow_words;
    bool              cts_inverted_latched;
} v24_runtime_t;

static v24_runtime_t  v24_runtime = {.rx_dma_chan = -1};
static v24_tx_state_t v24_tx_state = {
    .phase               = V24_TX_PHASE_IDLE,
    .cts_restart_enabled = true,
};
static v24_tx_event_t v24_tx_pending_event;
static uint32_t rx_dma_ring[RX_DMA_RING_WORDS] __attribute__((aligned(RX_DMA_RING_BYTES)));

static bool v24_tx_get_cts_level(void)
{
    bool sampled = gpio_get(V24_CTS) != 0;
    return v24_runtime.cts_inverted_latched ? !sampled : sampled;
}

uint32_t rx_dma_get_overflow_words(void)
{
    return v24_runtime.rx_dma_overflow_words;
}

static void rx_dma_configure_and_start(int channel, PIO pio, uint pio_sm)
{
    dma_channel_config dma_cfg = dma_channel_get_default_config((uint)channel);
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_cfg, false);
    channel_config_set_write_increment(&dma_cfg, true);
    channel_config_set_ring(&dma_cfg, true, RX_DMA_RING_BITS);
    channel_config_set_dreq(&dma_cfg, pio_get_dreq(pio, pio_sm, false));

    dma_channel_configure((uint)channel, &dma_cfg, rx_dma_ring, &pio->rxf[pio_sm], UINT32_MAX,
                          true);
}

static bool rx_dma_init(PIO pio, uint pio_sm)
{
    if (v24_runtime.rx_dma_chan >= 0)
    {
        dma_channel_abort((uint)v24_runtime.rx_dma_chan);
        dma_channel_unclaim((uint)v24_runtime.rx_dma_chan);
        v24_runtime.rx_dma_chan = -1;
    }

    int channel = dma_claim_unused_channel(false);
    if (channel < 0)
    {
        v24_runtime.rx_dma_enabled = false;
        LOG_ERROR("RX DMA: no free channel, fallback to direct FIFO polling\r\n");
        return false;
    }

    memset(rx_dma_ring, 0, sizeof(rx_dma_ring));
    rx_dma_configure_and_start(channel, pio, pio_sm);
    v24_runtime.rx_dma_chan           = channel;
    v24_runtime.rx_dma_enabled        = true;
    v24_runtime.rx_dma_words_consumed = 0U;
    v24_runtime.rx_dma_overflow_words = 0U;
    LOG_INFO("RX DMA: enabled chan=%d ring=%u bytes\r\n", channel, (unsigned)RX_DMA_RING_BYTES);
    return true;
}

static void rx_dma_restart(void)
{
    if (!v24_runtime.rx_dma_enabled || v24_runtime.rx_dma_chan < 0 || v24_runtime.rx_pio == NULL)
    {
        return;
    }

    dma_channel_abort((uint)v24_runtime.rx_dma_chan);
    memset(rx_dma_ring, 0, sizeof(rx_dma_ring));
    rx_dma_configure_and_start(v24_runtime.rx_dma_chan, v24_runtime.rx_pio, v24_runtime.rx_sm);
    v24_runtime.rx_dma_words_consumed = 0U;
}

static uint32_t rx_dma_words_produced(void)
{
    if (!v24_runtime.rx_dma_enabled || v24_runtime.rx_dma_chan < 0)
    {
        return 0U;
    }
    uint32_t remaining = dma_channel_hw_addr((uint)v24_runtime.rx_dma_chan)->transfer_count;
    return UINT32_MAX - remaining;
}

static void cts_raw_irq_handler(void)
{
    uint32_t events = gpio_get_irq_event_mask(V24_CTS);
    uint32_t mask   = GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE;
    if (events & mask)
    {
        gpio_acknowledge_irq(V24_CTS, events & mask);
        v24_runtime.cts_toggle_seq++;
        if (gpio_get(V24_CTS) != 0)
        {
            v24_runtime.rx_frame_start_seq++;
        }
    }
}

static void cts_irq_init(void)
{
    static bool initialized = false;
    if (!initialized)
    {
        gpio_add_raw_irq_handler_masked(1U << V24_CTS, cts_raw_irq_handler);
        initialized = true;
    }

    gpio_set_irq_enabled(V24_CTS, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    irq_set_enabled(IO_IRQ_BANK0, true);
}

// NOLINTBEGIN(misc-include-cleaner)
// These are actualy visible and clang-tidy has an issue
// with the pico-sdk here
static gpio_function_t pio_gpio_func(PIO pio)
{
    if (pio == pio0)
    {
        return GPIO_FUNC_PIO0;
    }
    if (pio == pio1)
    {
        return GPIO_FUNC_PIO1;
    }
    return GPIO_FUNC_NULL; // or assert(false)
}
// NOLINTEND(misc-include-cleaner)

static float baud_to_clockdiv(V24_BAUDRATE_T baudrate)
{
    return SYS_CLK_HZ / (TX_PIO_CYCLES_PER_BIT * (float)baudrate);
}

#define LED_MIRROR_PIO pio0
void led_mirror_init(void)
{
    // For now this cant be used as pin 25 is also the
    // reset signal for w5500
    PIO pio    = LED_MIRROR_PIO; // keep pio1 free for W5500 PIO-SPI
    int pio_sm = pio_claim_unused_sm(pio, false);
    if (pio_sm < 0)
    {
        LOG_ERROR("LED mirror: no free SM on pio0\r\n");
        return;
    }
    LOG_INFO("LED Mirror: init pio%u sm%u \r\n", (unsigned)pio_get_index(pio), (unsigned)pio_sm);

    if (!pio_can_add_program(pio, &led_mirror_program))
    {
        LOG_ERROR("LED mirror: no room for program on pio0\r\n");
        pio_sm_unclaim(pio, (uint)pio_sm);
        return;
    }

    uint offset = pio_add_program(pio, &led_mirror_program);

    pio_sm_config cfg = led_mirror_program_get_default_config(offset);
    sm_config_set_set_pins(&cfg, V24_STATUS_LED, 1);
    sm_config_set_jmp_pin(&cfg, V24_TXD);
    sm_config_set_in_pins(&cfg, V24_RXD);
    sm_config_set_in_shift(&cfg, false, false, sizeof(uint32_t) * CHAR_BIT);

    pio_gpio_init(pio, V24_STATUS_LED);
    pio_sm_set_consecutive_pindirs(pio, (uint)pio_sm, V24_STATUS_LED, 1, true);

    // TXD/RXD stay inputs; no pio_gpio_init needed for read-only pin sampling.
    pio_sm_init(pio, (uint)pio_sm, offset, &cfg);
    pio_sm_set_enabled(pio, (uint)pio_sm, true);

    LOG_INFO("LED mirror: enabled on pio%u sm%d offset=%u\r\n", (unsigned)pio_get_index(pio),
             pio_sm, (unsigned)offset);
}

void reinit_v24_config(V24_CONFIG_T* config, V24_BAUDRATE_T baudrate)
{
    config->baudrate  = (baudrate >= V24_BAUD_1200) ? baudrate : V24_BAUD_1200;
    uint32_t t_bit_us = (US_PER_SECOND + (uint32_t)config->baudrate - 1U) /
                        (uint32_t)config->baudrate;
    uint32_t holdoff_us = V24_RTS_HOLDOFF_MARGIN * t_bit_us;
    if (holdoff_us < V24_RTS_MIN_HOLDOFF)
    {
        holdoff_us = V24_RTS_MIN_HOLDOFF;
    }
    if (v24_runtime.tx_pio != NULL)
    {
        gpio_put(V24_RTS, 0);
    }
    v24_tx_state_reconfigure(&v24_tx_state, holdoff_us);
    v24_tx_pending_event = V24_TX_EVENT_NONE;
}

void init_v24_config(V24_CONFIG_T* config, V24_BAUDRATE_T baudrate)
{
    config->polarities = init_polarities();
    reinit_v24_config(config, baudrate);
}

bool rx_clock_poll_stall(void)
{
    if (v24_runtime.rx_pio != NULL)
    {
        uint32_t rx_stall_mask = (1U << (PIO_FDEBUG_RXSTALL_LSB + v24_runtime.rx_sm));
        if ((v24_runtime.rx_pio->fdebug & rx_stall_mask) != 0U)
        {
            v24_runtime.rx_pio->fdebug = rx_stall_mask;
            return true;
        }
    }
    return false;
}

bool rx_get(uint8_t* data)
{
    if (!data)
    {
        return false;
    }
    if (v24_runtime.rx_dma_enabled)
    {
        uint32_t produced_words = rx_dma_words_produced();
        uint32_t consumed_words = v24_runtime.rx_dma_words_consumed;
        uint32_t available      = produced_words - consumed_words;
        if (available == 0U)
        {
            return false;
        }

        if (available > RX_DMA_RING_WORDS)
        {
            uint32_t overflow_words = available - RX_DMA_RING_WORDS;
            v24_runtime.rx_dma_overflow_words += overflow_words;
            consumed_words = produced_words - RX_DMA_RING_WORDS;
        }

        uint32_t index = consumed_words & (RX_DMA_RING_WORDS - 1U);
        *data = (uint8_t)(rx_dma_ring[index] >> RX_SHIFT_TO_LSB);
        v24_runtime.rx_dma_words_consumed = consumed_words + 1U;
        return true;
    }

    assert(v24_runtime.rx_pio != NULL);
    if (v24_runtime.rx_pio == NULL ||
        pio_sm_is_rx_fifo_empty(v24_runtime.rx_pio, v24_runtime.rx_sm))
    {
        return false;
    }
    *data = (uint8_t)(pio_sm_get(v24_runtime.rx_pio, v24_runtime.rx_sm) >> RX_SHIFT_TO_LSB);
    return true;
}
void rx_clock_update_settings(V24_RX_POLARITIES_T* polarities)
{
    assert(v24_runtime.rx_pio != NULL);
    pio_sm_set_enabled(v24_runtime.rx_pio, v24_runtime.rx_sm, false);

    if (polarities)
    {
        gpio_set_inover(V24_RXC,
                        polarities->rxc_inverted ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
        gpio_set_inover(V24_RXD,
                        polarities->rxd_inverted ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
        gpio_set_inover(V24_DCD,
                        polarities->dcd_inverted ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
    }

    pio_sm_set_enabled(v24_runtime.rx_pio, v24_runtime.rx_sm, true);
}

void rx_clock_hard_reset(void)
{
    if (v24_runtime.rx_pio != NULL)
    {
        if (v24_runtime.rx_dma_enabled && v24_runtime.rx_dma_chan >= 0)
        {
            dma_channel_abort((uint)v24_runtime.rx_dma_chan);
        }
        pio_sm_set_enabled(v24_runtime.rx_pio, v24_runtime.rx_sm, false);
        pio_sm_clear_fifos(v24_runtime.rx_pio, v24_runtime.rx_sm);
        pio_sm_restart(v24_runtime.rx_pio, v24_runtime.rx_sm);
        pio_sm_clkdiv_restart(v24_runtime.rx_pio, v24_runtime.rx_sm);
        pio_sm_set_enabled(v24_runtime.rx_pio, v24_runtime.rx_sm, true);
        rx_dma_restart();
    }
}

bool rx_clock_poll_frame_start(void)
{
    uint32_t frame_start_seq = v24_runtime.rx_frame_start_seq;
    if (frame_start_seq == v24_runtime.rx_frame_start_seen)
    {
        return false;
    }

    v24_runtime.rx_frame_start_seen = frame_start_seq;
    rx_clock_hard_reset();
    return true;
}

void rx_clock_init(PIO pio, uint pio_sm, V24_RX_POLARITIES_T* polarities)
{
    LOG_INFO("RXC: init pio%u sm%u pin%u\r\n", (unsigned)pio_get_index(pio), (unsigned)pio_sm,
             (unsigned)V24_RXC);

    pio_sm_claim(pio, pio_sm);
    v24_runtime.rx_pio = pio;
    v24_runtime.rx_sm  = pio_sm;
    // Load PIO program
    uint offset = pio_add_program(pio, &rck_rxd_program);
    LOG_INFO("RXC: program offset=%u\r\n", (unsigned)offset);

    // Route GPIO to PIO
    pio_gpio_init(pio, V24_RXD);
    pio_sm_set_consecutive_pindirs(pio, pio_sm, V24_RXD, 1, false);
    pio_gpio_init(pio, V24_RXC);
    pio_sm_set_consecutive_pindirs(pio, pio_sm, V24_RXC, 1, false);

    // Configure state machine
    pio_sm_config config = rck_rxd_program_get_default_config(offset);
    sm_config_set_jmp_pin(&config, V24_RXC);
    sm_config_set_in_pins(&config, V24_RXD);
    sm_config_set_in_shift(&config, true, true, CHAR_BIT);
    // External clock can keep streaming continuously; use joined RX FIFO for more headroom.
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_RX);
    pio_sm_init(pio, pio_sm, offset, &config);

    // This implicitly enables the sm
    rx_clock_update_settings(polarities);
    (void)rx_dma_init(pio, pio_sm);
    LOG_INFO("RXC: enabled\r\n");
}

static uint32_t v24_tx_stall_mask(void)
{
    return 1U << (PIO_FDEBUG_TXSTALL_LSB + v24_runtime.tx_sm);
}

static bool v24_tx_pio_stalled(void)
{
    return v24_runtime.tx_pio != NULL &&
           (v24_runtime.tx_pio->fdebug & v24_tx_stall_mask()) != 0U;
}

static void v24_tx_reset_pio(void)
{
    if (v24_runtime.tx_pio == NULL)
    {
        return;
    }

    pio_sm_set_enabled(v24_runtime.tx_pio, v24_runtime.tx_sm, false);
    pio_sm_clear_fifos(v24_runtime.tx_pio, v24_runtime.tx_sm);
    pio_sm_restart(v24_runtime.tx_pio, v24_runtime.tx_sm);
    pio_sm_clkdiv_restart(v24_runtime.tx_pio, v24_runtime.tx_sm);
    pio_sm_set_enabled(v24_runtime.tx_pio, v24_runtime.tx_sm, true);
}

static void v24_tx_apply_actions(v24_tx_action_t actions)
{
    if ((actions & V24_TX_ACTION_RESTART_FRAME) != 0U)
    {
        if (v24_tx_state.restart_reason == V24_TX_RESTART_UNDERRUN)
        {
            v24_runtime.tx_fifo_underrun_count++;
        }
        else if (v24_tx_state.restart_reason == V24_TX_RESTART_CTS)
        {
            v24_runtime.tx_cts_restart_count++;
        }
    }

    if ((actions & V24_TX_ACTION_RESET_PIO) != 0U)
    {
        v24_tx_reset_pio();
    }
    if ((actions & V24_TX_ACTION_CLEAR_STALL) != 0U &&
        (actions & V24_TX_ACTION_ALLOW_WRITE) == 0U && v24_runtime.tx_pio != NULL)
    {
        v24_runtime.tx_pio->fdebug = v24_tx_stall_mask();
    }
    if ((actions & V24_TX_ACTION_ASSERT_RTS) != 0U)
    {
        gpio_set_dir(V24_RTS, GPIO_OUT);
        gpio_put(V24_RTS, 1);
    }
    if ((actions & V24_TX_ACTION_DEASSERT_RTS) != 0U)
    {
        gpio_put(V24_RTS, 0);
    }
}

static v24_tx_event_t v24_tx_event_from_actions(v24_tx_action_t actions)
{
    if ((actions & V24_TX_ACTION_RESTART_FRAME) != 0U)
    {
        return V24_TX_EVENT_RESTART_FRAME;
    }
    if ((actions & V24_TX_ACTION_FRAME_COMPLETE) != 0U)
    {
        return V24_TX_EVENT_FRAME_COMPLETE;
    }
    return V24_TX_EVENT_NONE;
}

bool v24_tx_try_write(uint8_t data)
{
    assert(v24_runtime.tx_pio != NULL);
    if (v24_runtime.tx_pio == NULL ||
        pio_sm_is_tx_fifo_full(v24_runtime.tx_pio, v24_runtime.tx_sm))
    {
        return false;
    }

    uint64_t now_us = to_us_since_boot(get_absolute_time());
    v24_tx_action_t actions = v24_tx_state_request_write(
        &v24_tx_state, now_us, v24_runtime.cts_toggle_seq, v24_tx_pio_stalled());
    v24_tx_apply_actions(actions);
    v24_tx_pending_event = v24_tx_event_from_actions(actions);
    if ((actions & V24_TX_ACTION_ALLOW_WRITE) == 0U)
    {
        return false;
    }

    pio_sm_put(v24_runtime.tx_pio, v24_runtime.tx_sm, data);
    if ((actions & V24_TX_ACTION_CLEAR_STALL) != 0U)
    {
        /*
         * A state machine blocked on its initial pull can reassert TXSTALL
         * between clearing the sticky bit and filling the FIFO. Clear after
         * the first write so only a later empty-FIFO event is an underrun.
         */
        v24_runtime.tx_pio->fdebug = v24_tx_stall_mask();
    }
    return true;
}

void v24_tx_mark_frame_queued(void)
{
    v24_tx_apply_actions(v24_tx_state_frame_queued(&v24_tx_state));
}

v24_tx_event_t v24_tx_poll(void)
{
    if (v24_tx_pending_event != V24_TX_EVENT_NONE)
    {
        v24_tx_event_t event = v24_tx_pending_event;
        v24_tx_pending_event = V24_TX_EVENT_NONE;
        return event;
    }

    uint64_t now_us = to_us_since_boot(get_absolute_time());
    v24_tx_action_t actions =
        v24_tx_state_poll(&v24_tx_state, now_us, v24_runtime.cts_toggle_seq,
                          v24_tx_pio_stalled());
    v24_tx_apply_actions(actions);
    return v24_tx_event_from_actions(actions);
}

void v24_tx_set_cts_restart_enabled(bool enabled)
{
    v24_tx_state_set_cts_restart(&v24_tx_state, enabled);
}

bool v24_tx_get_cts_restart_enabled(void)
{
    return v24_tx_state.cts_restart_enabled;
}

void v24_tx_status_snapshot(v24_tx_status_t* status)
{
    if (status == NULL)
    {
        return;
    }

    status->pio_stalled         = v24_tx_pio_stalled();
    status->tx_fifo_underruns   = v24_runtime.tx_fifo_underrun_count;
    status->tx_cts_restarts     = v24_runtime.tx_cts_restart_count;
    status->cts_level           = v24_tx_get_cts_level();
    status->cts_edge_count      =
        v24_runtime.cts_toggle_seq - v24_runtime.cts_statistics_baseline;
    status->cts_restart_enabled = v24_tx_state.cts_restart_enabled;
}

void v24_reset_statistics(void)
{
    v24_runtime.tx_fifo_underrun_count  = 0U;
    v24_runtime.tx_cts_restart_count    = 0U;
    v24_runtime.rx_dma_overflow_words   = 0U;
    v24_runtime.cts_statistics_baseline = v24_runtime.cts_toggle_seq;
}

void tx_clock_update_settings(V24_CONFIG_T* config)
{
    assert(v24_runtime.tx_pio != NULL);
    float clkdiv = baud_to_clockdiv(config->baudrate);

    pio_sm_set_enabled(v24_runtime.tx_pio, v24_runtime.tx_sm, false);

    V24_TX_POLARITIES_T* polarities = &(config->polarities.tx_polarities);

    if (config->external_clock)
    {
        gpio_set_inover(V24_TXC_DCE,
                        polarities->txc_inverted ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
    }
    else
    {
        gpio_set_outover(V24_TXC_DTE,
                         polarities->txc_inverted ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
    }
    gpio_set_outover(V24_TXD,
                     polarities->txd_inverted ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
    gpio_set_outover(V24_DTR,
                     polarities->dtr_inverted ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
    gpio_set_outover(V24_RTS,
                     polarities->rts_inverted ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
    gpio_set_inover(V24_CTS,
                    polarities->cts_inverted ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
    v24_runtime.cts_inverted_latched = polarities->cts_inverted;

    if (!config->external_clock)
    {
        pio_sm_set_clkdiv(v24_runtime.tx_pio, v24_runtime.tx_sm, clkdiv);
    }
    else
    {
        // pio_sm_set_clkdiv(v24_runtime.tx_pio, v24_runtime.tx_sm, 1.0f);
    }
    /*
     * Runtime updates happen only at a physical frame boundary. Restart the
     * SM and divider so the next frame cannot inherit a stalled instruction
     * or fractional divider phase from the previous baud/polarity setting.
     */
    pio_sm_clear_fifos(v24_runtime.tx_pio, v24_runtime.tx_sm);
    pio_sm_restart(v24_runtime.tx_pio, v24_runtime.tx_sm);
    pio_sm_clkdiv_restart(v24_runtime.tx_pio, v24_runtime.tx_sm);
    v24_runtime.tx_pio->fdebug = v24_tx_stall_mask();
    pio_sm_set_enabled(v24_runtime.tx_pio, v24_runtime.tx_sm, true);
}

static void tx_clock_init_tck(PIO pio, uint pio_sm, V24_CONFIG_T* config)
{
    float clkdiv = baud_to_clockdiv(config->baudrate);

    LOG_INFO("Initializing internal clock\r\n");
    LOG_DEBUG("TXC: init pio%u sm%u pin%u baud=%u clkdiv=%.6f\r\n", (unsigned)pio_get_index(pio),
              (unsigned)pio_sm, (unsigned)V24_TXC_DTE, (unsigned)config->baudrate, (double)clkdiv);

    pio_sm_claim(pio, pio_sm);
    v24_runtime.tx_pio = pio;
    v24_runtime.tx_sm  = pio_sm;

    // Load PIO program
    uint offset = pio_add_program(pio, &tck_txd_program);
    LOG_DEBUG("TXC: program offset=%u\r\n", (unsigned)offset);

    // Route GPIO to PIO
    pio_gpio_init(pio, V24_TXC_DTE);
    pio_sm_set_consecutive_pindirs(pio, pio_sm, V24_TXC_DTE, 1, true);
    pio_gpio_init(pio, V24_TXD);
    pio_sm_set_consecutive_pindirs(pio, pio_sm, V24_TXD, 1, true);
    pio_gpio_init(pio, V24_CTS);
    pio_sm_set_consecutive_pindirs(pio, pio_sm, V24_CTS, 1, false);

    // Configure state machine
    pio_sm_config sm_config = tck_txd_program_get_default_config(offset);
    sm_config_set_sideset_pins(&sm_config, V24_TXC_DTE);
    sm_config_set_out_pins(&sm_config, V24_TXD, 1);
    sm_config_set_jmp_pin(&sm_config, V24_CTS);
    sm_config_set_out_shift(&sm_config, true, true, CHAR_BIT);
    sm_config_set_fifo_join(&sm_config, PIO_FIFO_JOIN_TX);

    pio_sm_init(pio, pio_sm, offset, &sm_config);

    // This implicitly activates the sm
    tx_clock_update_settings(config);
    LOG_INFO("Setting RTS Holdoff to %u us\r\n", (unsigned)v24_tx_state.holdoff_us);
    LOG_INFO("TXC: enabled\r\n");
}

static void tx_clock_init_xck(PIO pio, uint pio_sm, V24_CONFIG_T* config)
{
    LOG_INFO("Initializing external clock\r\n");
    LOG_DEBUG("TXC: init pio%u sm%u pin%u\r\n", (unsigned)pio_get_index(pio), (unsigned)pio_sm,
              (unsigned)V24_TXC_DCE);

    pio_sm_claim(pio, pio_sm);
    v24_runtime.tx_pio = pio;
    v24_runtime.tx_sm  = pio_sm;

    // Load PIO program
    uint offset = pio_add_program(pio, &xck_txd_program);
    LOG_DEBUG("XCK: program offset=%u\r\n", (unsigned)offset);

    // Route GPIO to PIO
    pio_gpio_init(pio, V24_TXC_DCE);
    pio_sm_set_consecutive_pindirs(pio, pio_sm, V24_TXC_DCE, 1, false);
    pio_gpio_init(pio, V24_TXD);
    pio_sm_set_consecutive_pindirs(pio, pio_sm, V24_TXD, 1, true);
    pio_gpio_init(pio, V24_CTS);
    pio_sm_set_consecutive_pindirs(pio, pio_sm, V24_CTS, 1, false);

    // Configure state machine
    pio_sm_config sm_config = xck_txd_program_get_default_config(offset);
    sm_config_set_in_pins(&sm_config, V24_TXC_DCE);
    sm_config_set_out_pins(&sm_config, V24_TXD, 1);
    sm_config_set_jmp_pin(&sm_config, V24_CTS);
    // xck_txd.pio uses explicit `pull block` and emits exactly 8 bits/byte.
    sm_config_set_out_shift(&sm_config, true, false, CHAR_BIT);
    // Eight TX words cover bounded core-0 work without starving a frame mid-byte stream.
    sm_config_set_fifo_join(&sm_config, PIO_FIFO_JOIN_TX);

    pio_sm_init(pio, pio_sm, offset, &sm_config);

    // This implicitly activates the sm
    tx_clock_update_settings(config);
    LOG_INFO("Setting RTS Holdoff to %u us\r\n", (unsigned)v24_tx_state.holdoff_us);
    LOG_INFO("XCK: enabled\r\n");
}

void tx_clock_init(PIO pio, uint pio_sm, V24_CONFIG_T* config)
{
    config->external_clock ? tx_clock_init_xck(pio, pio_sm, config)
                           : tx_clock_init_tck(pio, pio_sm, config);
    cts_irq_init();
}
