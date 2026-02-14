/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/platform/watchdog.h
 * Purpose: ALl watchdog interactions
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef WATCHDOG_H
#define WATCHDOG_H
/**
 * @brief Reboots by resetting the watchdog.
 */
void reboot(void);

#endif /* WATCHDOG_H */
