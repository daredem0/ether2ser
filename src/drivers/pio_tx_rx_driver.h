

/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/drivers/pio_tx_rx_driver.h
 * Purpose: PIO TX clock driver API and baudrate definitions.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef PIO_TX_RX_DRIVER_H
#define PIO_TX_RX_DRIVER_H
// Related headers

// Standard library headers
#include <stdbool.h>
#include <stdint.h>

// Library Headers
#include "hardware/pio.h"

// Project Headers
#include "drivers/gpio_driver.h"
#include "drivers/v24_config.h"

// Generated headers

typedef enum
{
    V24_TX_EVENT_NONE = 0,
    V24_TX_EVENT_RESTART_FRAME,
    V24_TX_EVENT_FRAME_COMPLETE,
} v24_tx_event_t;

typedef struct
{
    bool     pio_stalled;
    uint32_t tx_fifo_underruns;
    uint32_t tx_cts_restarts;
    bool     cts_level;
    uint32_t cts_edge_count;
    bool     cts_restart_enabled;
} v24_tx_status_t;

/**
 * @brief Initialize TX clock/data PIO state machine.
 * @param pio PIO instance.
 * @param pio_sm State machine index.
 * @param config V.24 configuration.
 */
void tx_clock_init(PIO pio, uint pio_sm, V24_CONFIG_T* config);

/**
 * @brief Try to queue one byte to the V.24 TX path.
 * @param data Byte to transmit.
 * @return true only when the byte was written to the PIO FIFO.
 */
bool v24_tx_try_write(uint8_t data);

/** @brief Mark the final byte of the active frame as queued. */
void v24_tx_mark_frame_queued(void);

/**
 * @brief Advance the physical TX lifecycle.
 * @return Queue-facing restart or completion event, if any.
 */
v24_tx_event_t v24_tx_poll(void);

/** @brief Configure CTS-edge-triggered frame restart policy. */
void v24_tx_set_cts_restart_enabled(bool enabled);

/** @brief Return the current CTS-edge-triggered restart policy. */
bool v24_tx_get_cts_restart_enabled(void);

/** @brief Copy TX diagnostics without exposing PIO internals. */
void v24_tx_status_snapshot(v24_tx_status_t* status);

/**
 * @brief Initialize RX clock/data PIO state machine.
 * @param pio PIO instance.
 * @param pio_sm State machine index.
 * @param polarities RX signal polarity configuration.
 */
void rx_clock_init(PIO pio, uint pio_sm, V24_RX_POLARITIES_T* polarities);

/**
 * @brief Read one received byte from RX PIO FIFO.
 * @param data Destination pointer for the received byte.
 * @return true if a byte was read, false if RX FIFO is empty.
 */
bool rx_get(uint8_t* data);

/**
 * @brief Initialize V.24 runtime configuration structure.
 * @param config Destination configuration object.
 * @param baudrate Initial baudrate.
 */
void init_v24_config(V24_CONFIG_T* config, V24_BAUDRATE_T baudrate);

/**
 * @brief Reinitialize V.24 runtime configuration and derived timing values.
 * @param config Destination configuration object.
 * @param baudrate New baudrate.
 */
void reinit_v24_config(V24_CONFIG_T* config, V24_BAUDRATE_T baudrate);

/**
 * @brief Initialize optional LED activity mirror PIO program.
 */
void led_mirror_init(void);

/**
 * @brief Apply TX runtime settings to an already configured TX PIO SM.
 * Take note that this function relies on tx_clock_init being called before.
 * This is asserted in debug builds but otherwise UB in production.
 * @param config V.24 configuration.
 */
void tx_clock_update_settings(V24_CONFIG_T* config);

/**
 * @brief Apply RX runtime settings to an already configured RX PIO SM.
 * Take note that this function relies on tx_clock_init being called before.
 * This is asserted in debug builds but otherwise UB in production.
 * @param polarities RX polarity settings.
 */
void rx_clock_update_settings(V24_RX_POLARITIES_T* polarities);


/**
 * @brief Disable RX Path, clear fifos, restart SMs and CLKDIV and enable again.
 */
void rx_clock_hard_reset(void);

/**
 * @brief Reset RX byte assembly after a peer RTS assertion and before its clock starts.
 * @return True when a pending frame-start edge caused an RX reset.
 */
bool rx_clock_poll_frame_start(void);

/**
 * @brief Poll RX FIFO stall events and update the provided event counter.
 * @return True if stalled, false otherwise.
 */
bool rx_clock_poll_stall(void);

/**
 * @brief Number of RX words dropped because DMA ring was overrun.
 * @return Cumulative dropped RX words.
 */
uint32_t rx_dma_get_overflow_words(void);

#endif /* PIO_TX_RX_DRIVER_H */
