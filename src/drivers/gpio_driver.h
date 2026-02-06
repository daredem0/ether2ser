/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/drivers/gpio_driver.h
 * Purpose: GPIO initialization and polarity configuration API.
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef GPIO_DRIVER_H
#define GPIO_DRIVER_H

// Related headers

// Standard library headers
#include <stdbool.h>

// Project Headers
#include "drivers/v24_config.h"

// Generated headers

V24_POLARITIES_T init_polarities(void);
void             init_pins(void);

#endif /* GPIO_DRIVER_H */
