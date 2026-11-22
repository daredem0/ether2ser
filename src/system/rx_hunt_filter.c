#include "system/rx_hunt_filter.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "protocol/hdlc_sync.h"

static bool rx_hunt_filter_is_idle(uint8_t byte)
{
    if (byte == 0x00U || byte == 0xFFU)
    {
        return true;
    }
    return false;
}

bool rx_hunt_filter_should_drop(bool external_clock, HDLC_SYNC_STATE_T sync_state,
                                size_t buffered_length, uint8_t buffered_tail, uint8_t byte)
{
    if (!external_clock || sync_state != HDLC_SYNC_STATE_HUNTING || buffered_length != 1U ||
        !rx_hunt_filter_is_idle(buffered_tail) || byte != buffered_tail)
    {
        return false;
    }
    return true;
}
