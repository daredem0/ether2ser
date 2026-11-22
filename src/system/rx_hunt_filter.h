#ifndef RX_HUNT_FILTER_H
#define RX_HUNT_FILTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "protocol/hdlc_sync.h"

/**
 * @brief Decide whether a newly received byte is redundant external-clock idle.
 *
 * Only a byte that repeats the single retained idle overlap may be dropped.
 * Once a possible opening flag has entered the accumulator, every following
 * byte is preserved even if the synchronizer has not polled the burst yet.
 */
bool rx_hunt_filter_should_drop(bool external_clock, HDLC_SYNC_STATE_T sync_state,
                                size_t buffered_length, uint8_t buffered_tail, uint8_t byte);

#endif /* RX_HUNT_FILTER_H */
