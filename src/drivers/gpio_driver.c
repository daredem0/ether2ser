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
#include "drivers/gpio_control.h"
#include "drivers/v24_config.h"
#include "platform/pinmap.h"
#include "system/common.h"
#include "system/operating_mode.h"

// Generated headers


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

void gpio_driver_init(operating_mode_t mode)
{
    LOG_DEBUG("Initializing GPIO ownership for %s mode\r\n", operating_mode_name(mode));
    for (size_t i = 0U; i < gpio_control_signal_count(); i++)
    {
        const gpio_signal_info_t* info = gpio_control_signal_at(i);
        if (info == NULL || info->reserved)
        {
            continue;
        }

        gpio_init(info->gpio_num);
        if (info->direction == GPIO_SIGNAL_OUTPUT)
        {
            gpio_put(info->gpio_num, 0);
            gpio_set_dir(info->gpio_num, GPIO_OUT);
        }
        else
        {
            gpio_set_dir(info->gpio_num, GPIO_IN);
            gpio_pull_down(info->gpio_num);
        }
    }
}

bool gpio_driver_set_manual_output(operating_mode_t mode, gpio_signal_t signal, bool value)
{
    if (!gpio_control_manual_write_allowed(mode, signal))
    {
        return false;
    }

    for (size_t i = 0U; i < gpio_control_signal_count(); i++)
    {
        const gpio_signal_info_t* info = gpio_control_signal_at(i);
        if (info != NULL && info->signal == signal)
        {
            gpio_put(info->gpio_num, value);
            return true;
        }
    }
    return false;
}
