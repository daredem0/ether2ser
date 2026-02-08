/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/drivers/pio_tx_rx_driver.c
 * Purpose: PIO TX/RX clock driver implementation.
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "pio_tx_rx_driver.h"

// Standard library headers
#include <stdint.h>
#include <stdio.h>

// Project Headers
#include "drivers/gpio_driver.h"
#include "hardware/pio.h"
#include "pico/time.h"
#include "pico/types.h"
#include "platform/pinmap.h"
#include "system/common.h"

// Generated headers
#include "led_activity_mirror.pio.h"
#include "rck_rxd.pio.h"
#include "tck_txd.pio.h"

typedef struct
{
    uint32_t tx_rts_holdoff_us;
    bool     rts_set;
} v24_runtime_t;

// This struct holds the runtime state of the v24 and shall not be exposed
v24_runtime_t v24_runtime;

// #define TX_RTS_HOLDOFF_US 4220u
// static bool rts_set = false;

static float baud_to_clockdiv(V24_BAUDRATE_T baudrate)
{
    return 125000000.0f / (3.0f * (float)baudrate);
}

#define LED_MIRROR_PIO pio0
void led_mirror_init(void)
{
    // For now this cant be used as pin 25 is also the
    // reset signal for w5500
    PIO pio = LED_MIRROR_PIO; // keep pio1 free for W5500 PIO-SPI
    int sm  = pio_claim_unused_sm(pio, false);
    if (sm < 0)
    {
        printf("LED mirror: no free SM on pio0\r\n");
        return;
    }
    LOG_INFO("LED Mirror: init pio%u sm%u \r\n", (unsigned)pio_get_index(pio), (unsigned)sm);

    if (!pio_can_add_program(pio, &led_mirror_program))
    {
        printf("LED mirror: no room for program on pio0\r\n");
        pio_sm_unclaim(pio, (uint)sm);
        return;
    }

    uint offset = pio_add_program(pio, &led_mirror_program);

    pio_sm_config cfg = led_mirror_program_get_default_config(offset);
    sm_config_set_set_pins(&cfg, V24_STATUS_LED, 1);
    sm_config_set_jmp_pin(&cfg, V24_TXD);
    sm_config_set_in_pins(&cfg, V24_RXD);
    sm_config_set_in_shift(&cfg, false, false, 32);

    pio_gpio_init(pio, V24_STATUS_LED);
    pio_sm_set_consecutive_pindirs(pio, (uint)sm, V24_STATUS_LED, 1, true);

    // TXD/RXD stay inputs; no pio_gpio_init needed for read-only pin sampling.
    pio_sm_init(pio, (uint)sm, offset, &cfg);
    pio_sm_set_enabled(pio, (uint)sm, true);

    printf("LED mirror: enabled on pio%u sm%d offset=%u\r\n", (unsigned)pio_get_index(pio), sm,
           (unsigned)offset);
}

void reinit_v24_config(V24_CONFIG_T* config, V24_BAUDRATE_T baudrate)
{
    config->baudrate              = baudrate;
    uint32_t t_bit_us             = (1000000u + (uint32_t)baudrate - 1u) / (uint32_t)baudrate;
    v24_runtime.tx_rts_holdoff_us = 41u * t_bit_us; // start conservative
    if (v24_runtime.tx_rts_holdoff_us < 200u)
    {
        v24_runtime.tx_rts_holdoff_us = 200u;
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
    if (pio0 == NULL || pio_sm_is_rx_fifo_empty(pio0, 1))
    {
        return false;
    }
    *data = (pio_sm_get(pio0, 1) >> 24);
    return true;
}

void rx_clock_init(PIO pio, uint pio_sm, V24_RX_POLARITIES_T* polarities)
{

    LOG_INFO("RXC: init pio%u sm%u pin%u\r\n", (unsigned)pio_get_index(pio), (unsigned)pio_sm,
             (unsigned)V24_RXC);

    pio_sm_claim(pio, pio_sm);
    // Load PIO program
    uint offset = pio_add_program(pio, &rck_rxd_program);
    LOG_INFO("RXC: program offset=%u\r\n", (unsigned)offset);

    // Route GPIO to PIO
    pio_gpio_init(pio, V24_RXD);
    pio_sm_set_consecutive_pindirs(pio, pio_sm, V24_RXD, 1, false);
    pio_gpio_init(pio, V24_RXC);
    pio_sm_set_consecutive_pindirs(pio, pio_sm, V24_RXC, 1, false);
    if (polarities != NULL)
    {
        if (polarities->rxc_inverted)
        {
            gpio_set_inover(V24_RXC, GPIO_OVERRIDE_INVERT);
        }
        if (polarities->rxd_inverted)
        {
            gpio_set_inover(V24_RXD, GPIO_OVERRIDE_INVERT);
        }
    }

    // Configure state machine
    pio_sm_config config = rck_rxd_program_get_default_config(offset);
    sm_config_set_jmp_pin(&config, V24_RXC);
    sm_config_set_in_pins(&config, V24_RXD);
    sm_config_set_in_shift(&config, true, true, 8);
    pio_sm_init(pio, pio_sm, offset, &config);
    pio_sm_set_enabled(pio, pio_sm, true);
    LOG_INFO("RXC: enabled\r\n");
}

static bool tx_can_deassert_rts(void)
{
    if (pio0 == NULL)
    {
        return false;
    }
    return pio_sm_is_tx_fifo_empty(pio0, 0);
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

    bool fifo_empty        = pio_sm_is_tx_fifo_empty(pio0, 0);
    bool stalled           = (pio0->fdebug & (1u << (PIO_FDEBUG_TXSTALL_LSB + 0))) != 0;
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
    return true;
}

bool tx_put(uint8_t data)
{
    if (!v24_runtime.rts_set)
    {
        // Indicate ready to send by setting RTS
        pio0->fdebug = (1u << (PIO_FDEBUG_TXSTALL_LSB + 0));
        gpio_set_dir(V24_RTS, GPIO_OUT);
        gpio_put(V24_RTS, 1);
        v24_runtime.rts_set = true;
    }
    if (pio0 == NULL || pio_sm_is_tx_fifo_full(pio0, 0))
    {
        return false;
    }
    pio_sm_put(pio0, 0, data);
    return true;
}

void tx_clock_init(PIO pio, uint pio_sm, V24_BAUDRATE_T baudrate, V24_TX_POLARITIES_T* polarities)
{
    float clkdiv = baud_to_clockdiv(baudrate);

    LOG_INFO("TXC: init pio%u sm%u pin%u baud=%u clkdiv=%.6f\r\n", (unsigned)pio_get_index(pio),
             (unsigned)pio_sm, (unsigned)V24_TXC_DTE, (unsigned)baudrate, (double)clkdiv);

    pio_sm_claim(pio, pio_sm);
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
    if (polarities != NULL)
    {
        if (polarities->txc_inverted)
        {
            gpio_set_outover(V24_TXC_DTE, GPIO_OVERRIDE_INVERT);
        }
        if (polarities->txd_inverted)
        {
            gpio_set_outover(V24_TXD, GPIO_OVERRIDE_INVERT);
        }
        if (polarities->cts_inverted)
        {
            gpio_set_inover(V24_CTS, GPIO_OVERRIDE_INVERT);
        }
    }

    // Configure state machine
    pio_sm_config config = tck_txd_program_get_default_config(offset);
    sm_config_set_sideset_pins(&config, V24_TXC_DTE);
    sm_config_set_out_pins(&config, V24_TXD, 1);
    sm_config_set_jmp_pin(&config, V24_CTS);
    sm_config_set_out_shift(&config, true, true, 8);
    sm_config_set_clkdiv(&config, clkdiv);
    pio_sm_init(pio, pio_sm, offset, &config);
    pio_sm_set_enabled(pio, pio_sm, true);
    LOG_INFO("Setting RTS Holdoff to %u us\r\n", (unsigned)v24_runtime.tx_rts_holdoff_us);

    LOG_INFO("TXC: enabled\r\n");
}
