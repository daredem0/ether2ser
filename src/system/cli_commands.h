/*
 * ether2ser — Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/cli_commands.h
 * Purpose: CLI command handler declarations.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef CLI_COMMANDS_H
#define CLI_COMMANDS_H

// Related headers

// Standard library headers

// Project Headers

// Generated headers

/**
 * @brief Parse and execute one CLI input line.
 * @param line Null-terminated command line.
 */
void        handle_cli_line(const char* line);

/**
 * @brief Return command name by table index.
 * @param index Command table index.
 * @return Command name string or `NULL` when out of range.
 */
const char* get_command_name(int index);

#endif /* CLI_COMMANDS_H */
