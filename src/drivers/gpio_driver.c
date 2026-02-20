/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/drivers/gpio_driver.c
 * Purpose: GPIO initialization and V.24 polarity defaults.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "gpio_driver.h"

// Standard library headers
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Library Headers
#include "hardware/gpio.h"

// Project Headers
#include "drivers/v24_config.h"
#include "platform/pinmap.h"
#include "system/common.h"

// Generated headers

V24_PIN_T outputs[] = {V24_RTS, V24_TXD, V24_DTR, V24_TXC_DTE, V24_TXC_DCE};

V24_PIN_T inputs[] = {V24_DCD, V24_DSR, V24_CTS, V24_RXD, V24_RXC};

V24_POLARITIES_T init_polarities(void)
{
    LOG_DEBUG("Initializing default polarities\r\n");
    return (V24_POLARITIES_T){.tx_polarities =
                                  {
                                      .txd_inverted = false,
                                      .txc_inverted = true,
                                      .cts_inverted = true,
                                      .rts_inverted = true,
                                      .dtr_inverted = true,
                                  },
                              .rx_polarities = {
                                  .rxd_inverted = false,
                                  .rxc_inverted = true,
                                  .dcd_inverted = true,
                              }};
}

void init_pins(void)
{
    for (size_t i = 0; i < ARRAY_LEN(inputs); i++)
    {
        gpio_init(inputs[i]);
        gpio_set_dir(inputs[i], GPIO_IN);
        gpio_pull_down(inputs[i]);
    }

    for (size_t i = 0; i < ARRAY_LEN(outputs); i++)
    {
        gpio_init(outputs[i]);
        gpio_set_dir(outputs[i], GPIO_OUT);
    }
}
