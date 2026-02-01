/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/cli_commands.h
 * Purpose: CLI command handler declarations.
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */


#ifndef CLI_COMMANDS_H
#define CLI_COMMANDS_H

void handle_cli_line(const char *line);
const char *get_command_name(int index);

#endif /* CLI_COMMANDS_H */
