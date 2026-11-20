#include <stdbool.h>
#include <stdint.h>

#include "drivers/pio_tx_rx_driver.h"
#include "mock_tx_queue_platform.h"
#include "pico/time.h"

PIO pio0 = (PIO)0;

static uint32_t cts_sequence;
static uint32_t hard_reset_count;

void mock_tx_queue_platform_reset(void)
{
    cts_sequence    = 0U;
    hard_reset_count = 0U;
}

void mock_tx_queue_set_cts_sequence(uint32_t sequence)
{
    cts_sequence = sequence;
}

uint32_t mock_tx_queue_hard_reset_count(void)
{
    return hard_reset_count;
}

bool pio_sm_is_tx_fifo_full(PIO pio, uint sm)
{
    (void)pio;
    (void)sm;
    return false;
}

bool tx_put(uint8_t data)
{
    (void)data;
    return true;
}

uint32_t tx_clock_get_cts_toggle_seq(void)
{
    return cts_sequence;
}

void tx_clock_hard_reset(void)
{
    hard_reset_count++;
}

absolute_time_t get_absolute_time(void)
{
    return 0U;
}

uint32_t to_ms_since_boot(absolute_time_t value)
{
    return (uint32_t)value;
}
