#include "system/operating_mode.h"

bool operating_mode_is_valid(operating_mode_t mode)
{
    return mode == OPERATING_MODE_BRIDGE || mode == OPERATING_MODE_GPIO;
}

operating_mode_t operating_mode_from_persisted(uint32_t stored_mode)
{
    const operating_mode_t mode = (operating_mode_t)stored_mode;
    return operating_mode_is_valid(mode) ? mode : OPERATING_MODE_DEFAULT;
}

const char* operating_mode_name(operating_mode_t mode)
{
    switch (mode)
    {
    case OPERATING_MODE_BRIDGE:
        return "bridge";
    case OPERATING_MODE_GPIO:
        return "gpio";
    default:
        return "invalid";
    }
}

bool operating_mode_change_allowed(operating_mode_t current_mode,
                                   operating_mode_t requested_mode,
                                   bool frame_active,
                                   bool queue_empty)
{
    if (!operating_mode_is_valid(current_mode) || !operating_mode_is_valid(requested_mode))
    {
        return false;
    }
    if (current_mode == requested_mode)
    {
        return true;
    }
    return !frame_active && queue_empty;
}
