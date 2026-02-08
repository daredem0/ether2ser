/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/baudrate_monitor.h
 * Purpose: RXC baudrate estimator interface.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef BAUDRATE_MONITOR_H
#define BAUDRATE_MONITOR_H

// Related headers

// Standard library headers

// Project Headers
#include "platform/pinmap.h"

// Generated headers

#define BAUDRATE_MONITOR_PIN V24_RXC

void baudrate_estimator_init(V24_PIN_T pin);
// void baudrate_estimator_poll(V24_PIN_T pin);
float baudrate_estimator_get_current_estimation(V24_PIN_T pin);

#endif /* BAUDRATE_MONITOR_H */
