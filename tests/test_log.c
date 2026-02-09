#include "system/common.h"
#include "unity.h"

void test_log_set_get_level_roundtrip(void)
{
    set_loglevel(LOG_LEVEL_ERROR);
    TEST_ASSERT_EQUAL(LOG_LEVEL_ERROR, get_loglevel());
    set_loglevel(LOG_LEVEL_TRACE);
    TEST_ASSERT_EQUAL(LOG_LEVEL_TRACE, get_loglevel());
}

void test_log_filtered_message_does_not_set_emitted_flag(void)
{
    set_loglevel(LOG_LEVEL_ERROR);
    (void)log_take_emitted_flag();
    LOG_INFO("[test] filtered info\r\n");
    TEST_ASSERT_FALSE(log_take_emitted_flag());
}

void test_log_emitted_message_sets_and_clears_flag(void)
{
    set_loglevel(LOG_LEVEL_INFO);
    (void)log_take_emitted_flag();
    LOG_INFO("[test] visible info\r\n");
    TEST_ASSERT_TRUE(log_take_emitted_flag());
    TEST_ASSERT_FALSE(log_take_emitted_flag());
}

void test_log_trace_visible_only_at_trace_level(void)
{
    set_loglevel(LOG_LEVEL_DEBUG);
    (void)log_take_emitted_flag();
    LOG_TRACE("[test] filtered trace\r\n");
    TEST_ASSERT_FALSE(log_take_emitted_flag());

    set_loglevel(LOG_LEVEL_TRACE);
    LOG_TRACE("[test] visible trace\r\n");
    TEST_ASSERT_TRUE(log_take_emitted_flag());
}
