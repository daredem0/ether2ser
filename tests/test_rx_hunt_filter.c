#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "protocol/hdlc_common.h"
#include "protocol/hdlc_sync.h"
#include "system/rx_hunt_filter.h"
#include "unity.h"

void test_rx_hunt_filter_drops_only_redundant_preframe_idle(void)
{
    TEST_ASSERT_TRUE(rx_hunt_filter_should_drop(true, HDLC_SYNC_STATE_HUNTING, 1U, 0x00U,
                                                0x00U));
    TEST_ASSERT_TRUE(rx_hunt_filter_should_drop(true, HDLC_SYNC_STATE_HUNTING, 1U, 0xFFU,
                                                0xFFU));
    TEST_ASSERT_FALSE(rx_hunt_filter_should_drop(true, HDLC_SYNC_STATE_HUNTING, 1U, 0x00U,
                                                 HDLC_FLAG_BYTE));
}

void test_rx_hunt_filter_preserves_repeated_zeroes_after_opening_flag(void)
{
    TEST_ASSERT_FALSE(rx_hunt_filter_should_drop(true, HDLC_SYNC_STATE_HUNTING, 2U,
                                                 HDLC_FLAG_BYTE, 0x00U));
    TEST_ASSERT_FALSE(rx_hunt_filter_should_drop(true, HDLC_SYNC_STATE_HUNTING, 3U, 0x00U,
                                                 0x00U));
    TEST_ASSERT_FALSE(rx_hunt_filter_should_drop(true, HDLC_SYNC_STATE_HUNTING, 4U, 0x00U,
                                                 0x00U));
}

void test_rx_hunt_filter_inactive_states_never_drop(void)
{
    TEST_ASSERT_FALSE(rx_hunt_filter_should_drop(false, HDLC_SYNC_STATE_HUNTING, 1U, 0x00U,
                                                 0x00U));
    TEST_ASSERT_FALSE(rx_hunt_filter_should_drop(true, HDLC_SYNC_STATE_SYNCED, 1U, 0x00U,
                                                 0x00U));
    TEST_ASSERT_FALSE(rx_hunt_filter_should_drop(true, HDLC_SYNC_STATE_HUNTING, 0U, 0x00U,
                                                 0x00U));
}
