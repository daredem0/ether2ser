/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/platform/watchdog.c
 * Purpose: ALl watchdog interactions
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "watchdog.h"

// Standard library headers
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Library Headers
#include "hardware/watchdog.h"
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"

// Project Headers
#include "system/common.h"
// Generated headers
void reboot(void)
{
    LOG_PLAIN("Rebooting...\r\n");
    watchdog_reboot(0, 0, FLUSH_LOG_BEFORE_REBOOT_MS); // small delay to let LOG_PLAIN flush
    // Do not return to the main loop; it calls watchdog_update() and would
    // keep postponing the reboot forever.
    while (true)
    {
        // wait for watchdog reset
    }
}
