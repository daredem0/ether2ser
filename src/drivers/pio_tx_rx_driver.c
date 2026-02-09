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
#include <stdint.h>
#include <stdio.h>

// Library Headers
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/platform_defs.h"
#include "hardware/regs/pio.h"
#include "pico/time.h"
#include "pico/types.h"

// Project Headers
#include "drivers/gpio_driver.h"
#include "drivers/v24_config.h"
#include "platform/pinmap.h"
#include "system/common.h"

// Generated headers
#include "led_activity_mirror.pio.h"
#include "rck_rxd.pio.h"
#include "tck_txd.pio.h"

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

// This struct holds the runtime state of the v24 and shall not be exposed
v24_runtime_t v24_runtime;

const v24_runtime_t* get_v24_runtime(void)
{
    return (const v24_runtime_t*)&v24_runtime;
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
    config->baudrate              = (baudrate >= V24_BAUD_1200) ? baudrate : V24_BAUD_1200;
    uint32_t t_bit_us             = (US_PER_SECOND + (uint32_t)baudrate - 1U) / (uint32_t)baudrate;
    v24_runtime.tx_rts_holdoff_us = V24_RTS_HOLDOFF_MARGIN * t_bit_us; // start conservative
    if (v24_runtime.tx_rts_holdoff_us < V24_RTS_MIN_HOLDOFF)
    {
        v24_runtime.tx_rts_holdoff_us = V24_RTS_MIN_HOLDOFF;
    }
    v24_runtime.rts_set = false;
}

void init_v24_config(V24_CONFIG_T* config, V24_BAUDRATE_T baudrate)
{
    config->polarities = init_polarities();
    reinit_v24_config(config, baudrate);
}

bool rx_get(uint8_t* data)
{
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
    }

    pio_sm_set_enabled(v24_runtime.rx_pio, v24_runtime.rx_sm, true);
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
    pio_sm_init(pio, pio_sm, offset, &config);

    // This implicitly enables the sm
    rx_clock_update_settings(polarities);
    LOG_INFO("RXC: enabled\r\n");
}

bool tx_poll(void)
{
    static bool     deassert_pending     = false;
    static uint64_t deassert_deadline_us = 0;

    if (!v24_runtime.rts_set)
    {
        deassert_pending = false;
        return true;
    }

    bool fifo_empty = pio_sm_is_tx_fifo_empty(v24_runtime.tx_pio, v24_runtime.tx_sm);
    bool stalled =
        (v24_runtime.tx_pio->fdebug & (1U << (PIO_FDEBUG_TXSTALL_LSB + v24_runtime.tx_sm))) != 0;
    bool ready_to_deassert = (fifo_empty || stalled);

    uint64_t now_us = to_us_since_boot(get_absolute_time());

    if (!ready_to_deassert)
    {
        deassert_pending = false;
        return false;
    }

    if (!deassert_pending)
    {
        deassert_pending     = true;
        deassert_deadline_us = now_us + v24_runtime.tx_rts_holdoff_us;
        return false;
    }

    if (now_us < deassert_deadline_us)
    {
        return false;
    }

    gpio_put(V24_RTS, 0);
    v24_runtime.rts_set = false;
    deassert_pending    = false;

    // AFTER RTS is deasserted we turn of the clock. Until then, we
    // have to keep clocking. To do that we temporarily take control over the pin
    gpio_set_function(V24_TXC_DTE, GPIO_FUNC_SIO); // NOLINT(misc-include-cleaner)
    gpio_set_dir(V24_TXC_DTE, GPIO_OUT);
    gpio_put(V24_TXC_DTE, 0);

    // Return control to pio, state machine is stalled, level will remain
    // until more bytes are pushed
    gpio_set_function(V24_TXC_DTE, pio_gpio_func(v24_runtime.tx_pio));
    return true;
}

bool tx_put(uint8_t data)
{
    assert(v24_runtime.tx_pio != NULL);
    if (v24_runtime.tx_pio == NULL)
    {
        return false;
    }
    if (!v24_runtime.rts_set)
    {
        // Indicate ready to send by setting RTS
        v24_runtime.tx_pio->fdebug = (1U << (PIO_FDEBUG_TXSTALL_LSB + v24_runtime.tx_sm));
        gpio_set_dir(V24_RTS, GPIO_OUT);
        gpio_put(V24_RTS, 1);
        v24_runtime.rts_set = true;
    }
    if (pio_sm_is_tx_fifo_full(v24_runtime.tx_pio, v24_runtime.tx_sm))
    {
        return false;
    }
    pio_sm_put(v24_runtime.tx_pio, v24_runtime.tx_sm, data);
    return true;
}

void tx_clock_update_settings(V24_BAUDRATE_T baudrate, V24_TX_POLARITIES_T* polarities)
{
    assert(v24_runtime.tx_pio != NULL);
    float clkdiv = baud_to_clockdiv(baudrate);

    pio_sm_set_enabled(v24_runtime.tx_pio, v24_runtime.tx_sm, false);

    if (polarities)
    {
        gpio_set_outover(V24_TXC_DTE,
                         polarities->txc_inverted ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
        gpio_set_outover(V24_TXD,
                         polarities->txd_inverted ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
        gpio_set_inover(V24_CTS,
                        polarities->cts_inverted ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
    }

    pio_sm_set_clkdiv(v24_runtime.tx_pio, v24_runtime.tx_sm, clkdiv);
    pio_sm_set_enabled(v24_runtime.tx_pio, v24_runtime.tx_sm, true);
}

void tx_clock_init(PIO pio, uint pio_sm, V24_BAUDRATE_T baudrate, V24_TX_POLARITIES_T* polarities)
{
    float clkdiv = baud_to_clockdiv(baudrate);

    LOG_INFO("TXC: init pio%u sm%u pin%u baud=%u clkdiv=%.6f\r\n", (unsigned)pio_get_index(pio),
             (unsigned)pio_sm, (unsigned)V24_TXC_DTE, (unsigned)baudrate, (double)clkdiv);

    pio_sm_claim(pio, pio_sm);
    v24_runtime.tx_pio = pio;
    v24_runtime.tx_sm  = pio_sm;

    // Load PIO program
    uint offset = pio_add_program(pio, &tck_txd_program);
    LOG_INFO("TXC: program offset=%u\r\n", (unsigned)offset);

    // Route GPIO to PIO
    pio_gpio_init(pio, V24_TXC_DTE);
    pio_sm_set_consecutive_pindirs(pio, pio_sm, V24_TXC_DTE, 1, true);
    pio_gpio_init(pio, V24_TXD);
    pio_sm_set_consecutive_pindirs(pio, pio_sm, V24_TXD, 1, true);
    pio_gpio_init(pio, V24_CTS);
    pio_sm_set_consecutive_pindirs(pio, pio_sm, V24_CTS, 1, false);

    // Configure state machine
    pio_sm_config config = tck_txd_program_get_default_config(offset);
    sm_config_set_sideset_pins(&config, V24_TXC_DTE);
    sm_config_set_out_pins(&config, V24_TXD, 1);
    sm_config_set_jmp_pin(&config, V24_CTS);
    sm_config_set_out_shift(&config, true, true, CHAR_BIT);

    pio_sm_init(pio, pio_sm, offset, &config);

    // This implicitly activates the sm
    tx_clock_update_settings(baudrate, polarities);
    LOG_INFO("Setting RTS Holdoff to %u us\r\n", (unsigned)v24_runtime.tx_rts_holdoff_us);
    LOG_INFO("TXC: enabled\r\n");
}
