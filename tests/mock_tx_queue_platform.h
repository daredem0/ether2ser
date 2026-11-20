#ifndef TESTS_MOCK_TX_QUEUE_PLATFORM_H
#define TESTS_MOCK_TX_QUEUE_PLATFORM_H

#include <stdint.h>

void mock_tx_queue_platform_reset(void);
void mock_tx_queue_set_cts_sequence(uint32_t sequence);
uint32_t mock_tx_queue_hard_reset_count(void);

#endif /* TESTS_MOCK_TX_QUEUE_PLATFORM_H */
