/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/event_loop.h
 * Purpose: Event loop entry-point declaration.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

// Related headers

// Standard library headers

// Library Headers

// Project Headers
#include "system/app_context.h"

// Generated headers

void event_loop(app_ctx_t* app);

#endif /* EVENT_LOOP_H */
