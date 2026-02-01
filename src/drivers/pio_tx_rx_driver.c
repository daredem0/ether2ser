/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/drivers/pio_tx_driver.c
 * Purpose: PIO TX clock driver implementation.
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "pio_tx_rx_driver.h"

// Standard library headers
#include <stdio.h>
#include <stdint.h>

// Library Headers
#include "pico/types.h"
#include "hardware/pio.h"

// Project Headers
#include "platform/pinmap.h"

// Generated headers
#include "tck_txd.pio.h"
#include "rck_rxd.pio.h"

static float baud_to_clockdiv(V24_BAUDRATE_T baudrate){
    return 125000000.0f / (3.0f * (float)baudrate);
}

bool rx_get(uint8_t *data){
    if(pio0 == NULL || pio_sm_is_rx_fifo_empty(pio0, 1)){
        return false;
    }
    *data = (pio_sm_get(pio0, 1) >> 24);
    return true;
}

void rx_clock_init(PIO pio, uint pio_sm, V24_RX_POLARITIES_T *polarities) {

    printf("RXC: init pio%u sm%u pin%u\r\n",
           (unsigned)pio_get_index(pio),
           (unsigned)pio_sm,
           (unsigned)V24_RXC);

    // Load PIO program
    uint offset = pio_add_program(pio, &rck_rxd_program);
    printf("RXC: program offset=%u\r\n", (unsigned)offset);

    // Route GPIO to PIO
    pio_gpio_init(pio, V24_RXD);
    pio_sm_set_consecutive_pindirs(pio, pio_sm, V24_RXD, 1, false);
    pio_gpio_init(pio, V24_RXC);
    pio_sm_set_consecutive_pindirs(pio, pio_sm, V24_RXC, 1, false);
    if( polarities != NULL ){
        if( polarities->rxc_inverted ){
            gpio_set_inover(V24_RXC, GPIO_OVERRIDE_INVERT);
        }
        if( polarities->rxd_inverted ){
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
    printf("RXC: enabled\r\n");
}

static bool rts_set = false;

bool tx_poll(){
    if (pio0->fdebug & (1u << (PIO_FDEBUG_TXSTALL_LSB + 0))) {
        // RTS deasserted, not ready to send
        gpio_put(V24_RTS, 0);
        rts_set = false;
        pio0->fdebug = (1u << (PIO_FDEBUG_TXSTALL_LSB + 0));
        return false;
    }
    return true;
}

bool tx_put(uint8_t data){
    if (!rts_set) {
        // Indicate ready to send by setting RTS
        pio0->fdebug = (1u << (PIO_FDEBUG_TXSTALL_LSB + 0));
        gpio_set_dir(V24_RTS, GPIO_OUT);
        gpio_put(V24_RTS, 1);
        rts_set = true;
    }
    if(pio0 == NULL || pio_sm_is_tx_fifo_full(pio0, 0)){
        return false;
    }
    pio_sm_put(pio0, 0, data);
    return true;
}

void tx_clock_init(PIO pio, uint pio_sm, V24_BAUDRATE_T baudrate, V24_TX_POLARITIES_T *polarities) {
    float clkdiv = baud_to_clockdiv(baudrate);

    printf("TXC: init pio%u sm%u pin%u baud=%u clkdiv=%.6f\r\n",
           (unsigned)pio_get_index(pio),
           (unsigned)pio_sm,
           (unsigned)V24_TXC_DTE,
           (unsigned)baudrate,
           (double)clkdiv);

    // Load PIO program
    uint offset = pio_add_program(pio, &tck_txd_program);
    printf("TXC: program offset=%u\r\n", (unsigned)offset);

    // Route GPIO to PIO
    pio_gpio_init(pio, V24_TXC_DTE);
    pio_sm_set_consecutive_pindirs(pio, pio_sm, V24_TXC_DTE, 1, true);
    pio_gpio_init(pio, V24_TXD);
    pio_sm_set_consecutive_pindirs(pio, pio_sm, V24_TXD, 1, true);
    pio_gpio_init(pio, V24_CTS);
    pio_sm_set_consecutive_pindirs(pio, pio_sm, V24_CTS, 1, false);
    if( polarities != NULL ){
        if( polarities->txc_inverted ){
            gpio_set_outover(V24_TXC_DTE, GPIO_OVERRIDE_INVERT);
        }
        if( polarities->txd_inverted ){
            gpio_set_outover(V24_TXD, GPIO_OVERRIDE_INVERT);
        }
        if( polarities->cts_inverted ){
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
    printf("TXC: enabled\r\n");
}
