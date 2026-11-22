#include <stdbool.h>
#include <stdint.h>

#include "system/event_scheduler.h"
#include "unity.h"

void test_event_scheduler_blocks_control_during_active_frame(void)
{
    TEST_ASSERT_EQUAL_UINT8(0U, event_scheduler_control_budget(true, false));
}

void test_event_scheduler_services_boundary_with_backlog(void)
{
    TEST_ASSERT_EQUAL_UINT8(2U, event_scheduler_control_budget(false, false));
}

void test_event_scheduler_services_full_idle_budget(void)
{
    TEST_ASSERT_EQUAL_UINT8(20U, event_scheduler_control_budget(false, true));
}

void test_event_scheduler_blocks_background_io_during_active_frame(void)
{
    TEST_ASSERT_FALSE(event_scheduler_background_io_allowed(true));
}

void test_event_scheduler_allows_background_io_at_frame_boundary(void)
{
    TEST_ASSERT_TRUE(event_scheduler_background_io_allowed(false));
}
