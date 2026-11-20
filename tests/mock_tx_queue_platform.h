#ifndef TESTS_MOCK_TX_QUEUE_PLATFORM_H
#define TESTS_MOCK_TX_QUEUE_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

void mock_tx_queue_platform_reset(void);
void mock_tx_queue_set_cts_sequence(uint32_t sequence);
void mock_tx_queue_set_frame_complete(bool complete);
void mock_tx_queue_set_poll_idle(bool idle);
uint32_t mock_tx_queue_frame_queued_count(void);
uint32_t mock_tx_queue_hard_reset_count(void);

#endif /* TESTS_MOCK_TX_QUEUE_PLATFORM_H */
