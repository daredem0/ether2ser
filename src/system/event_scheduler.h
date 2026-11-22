#ifndef EVENT_SCHEDULER_H
#define EVENT_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Return the bounded control-event budget for the current TX state.
 * @param frame_active True while a physical V.24 frame lifecycle is active.
 * @param queue_empty True when no queued frame remains.
 * @return Number of control events that may be dispatched at this control point.
 */
uint8_t event_scheduler_control_budget(bool frame_active, bool queue_empty);

/**
 * @brief Return whether potentially blocking background I/O may run.
 * @param frame_active True while a physical V.24 frame lifecycle is active.
 * @return false while TX refill latency must be minimized.
 */
bool event_scheduler_background_io_allowed(bool frame_active);

#endif /* EVENT_SCHEDULER_H */
