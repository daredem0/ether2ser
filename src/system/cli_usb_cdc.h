/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/cli_usb_cdc.h
 * Purpose: Interface for USB CDC CLI polling and line handling.
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef SYSTEM_CLI_USB_CDC_H
#define SYSTEM_CLI_USB_CDC_H

// Related headers

// Standard library headers

// Project Headers

// Generated headers

/**
 * @brief Poll USB CDC for CLI input, echo, and emit line events.
 */
void cli_poll(void);

/**
 * @brief Process one CLI line.
 * @param line Null-terminated line string.
 */
void handle_cli_line(const char *line);

#endif /* SYSTEM_CLI_USB_CDC_H */
