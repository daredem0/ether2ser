#include "system/event_scheduler.h"

#define EVENT_SCHEDULER_BOUNDARY_BUDGET 2U
#define EVENT_SCHEDULER_IDLE_BUDGET 20U

uint8_t event_scheduler_control_budget(bool frame_active, bool queue_empty)
{
    if (frame_active)
    {
        return 0U;
    }

    return queue_empty ? EVENT_SCHEDULER_IDLE_BUDGET : EVENT_SCHEDULER_BOUNDARY_BUDGET;
}

bool event_scheduler_background_io_allowed(bool frame_active)
{
    return !frame_active;
}
