
/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/event_dispatch.h
 * Purpose: Event dispatcher API for control-plane event handling.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef EVENT_DISPATCH_H
#define EVENT_DISPATCH_H

// Related headers

// Standard library headers

// Library Headers

// Project Headers
#include "system/app_context.h"
#include "system/event_queue.h"

// Generated headers
/**
 * @brief Dispatch one event to the corresponding handler.
 * @param event Event to process.
 * @param app Application context.
 */
void event_dispatch(event_t* event, app_ctx_t* app);

#endif /* EVENT_DISPATCH_H */
