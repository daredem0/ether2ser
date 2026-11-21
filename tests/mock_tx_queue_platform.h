#ifndef TESTS_MOCK_TX_QUEUE_PLATFORM_H
#define TESTS_MOCK_TX_QUEUE_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#include "drivers/pio_tx_rx_driver.h"

void mock_tx_queue_platform_reset(void);
void mock_tx_queue_set_event(v24_tx_event_t event);
void mock_tx_queue_set_write_enabled(bool enabled);
uint32_t mock_tx_queue_frame_queued_count(void);
uint32_t mock_tx_queue_poll_count(void);

#endif /* TESTS_MOCK_TX_QUEUE_PLATFORM_H */
