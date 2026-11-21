#include <stdbool.h>
#include <stdint.h>

#include "drivers/pio_tx_rx_driver.h"
#include "mock_tx_queue_platform.h"
#include "pico/time.h"

static v24_tx_event_t tx_event;
static bool           write_enabled;
static uint32_t       frame_queued_count;
static uint32_t       poll_count;

void mock_tx_queue_platform_reset(void)
{
    tx_event           = V24_TX_EVENT_NONE;
    write_enabled      = true;
    frame_queued_count = 0U;
    poll_count         = 0U;
}

void mock_tx_queue_set_event(v24_tx_event_t event)
{
    tx_event = event;
}

void mock_tx_queue_set_write_enabled(bool enabled)
{
    write_enabled = enabled;
}

uint32_t mock_tx_queue_frame_queued_count(void)
{
    return frame_queued_count;
}

uint32_t mock_tx_queue_poll_count(void)
{
    return poll_count;
}

bool v24_tx_try_write(uint8_t data)
{
    (void)data;
    return write_enabled;
}

void v24_tx_mark_frame_queued(void)
{
    frame_queued_count++;
}

v24_tx_event_t v24_tx_poll(void)
{
    poll_count++;
    v24_tx_event_t event = tx_event;
    tx_event             = V24_TX_EVENT_NONE;
    return event;
}

absolute_time_t get_absolute_time(void)
{
    return 0U;
}

uint32_t to_ms_since_boot(absolute_time_t value)
{
    return (uint32_t)value;
}
