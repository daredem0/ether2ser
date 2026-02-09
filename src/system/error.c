/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/error.c
 * Purpose: Handles panic states.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "error.h"

// Standard library headers
#include <stdbool.h>

// Library Headers
#include "hardware/watchdog.h"
#include "pico/platform/common.h"
#include "pico/time.h"

// Project Headers
#include "system/common.h"

// Generated headers

// Default network configuration

void fatal_panic(e2s_error_t reason)
{
    LOG_ERROR("[FATAL] reason=%d\r\n", (int)reason);

    // Optional: signal fatal state on status LED/GPIO here.

    // Preferred: reboot so system can recover cleanly.
    // delay_ms gives log time to flush on USB CDC.
    sleep_ms(200);
    watchdog_reboot(0, 0, 50); // reboot in ~50ms
    while (true)
    {
        tight_loop_contents();
    }
}
