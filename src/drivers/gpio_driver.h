/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/drivers/gpio_driver.h
 * Purpose: GPIO initialization and polarity configuration API.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef GPIO_DRIVER_H
#define GPIO_DRIVER_H

// Related headers

// Standard library headers
#include <stdbool.h>

// Project Headers
#include "drivers/gpio_control.h"
#include "drivers/v24_config.h"
#include "system/operating_mode.h"

// Generated headers

/**
 * @brief Build default V.24 polarity configuration.
 * @return Default polarity set for TX/RX/control lines.
 */
V24_POLARITIES_T init_polarities(void);

/**
 * @brief Initialize the SIO pin directions for one operating mode.
 * @param mode Selected startup ownership mode.
 */
void gpio_driver_init(operating_mode_t mode);

/**
 * @brief Drive a manual GPIO output when mode and signal policy permit it.
 * @return true when the output was written, false when ownership rejects it.
 */
bool gpio_driver_set_manual_output(operating_mode_t mode, gpio_signal_t signal, bool value);

#endif /* GPIO_DRIVER_H */
