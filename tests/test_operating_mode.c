#include <stddef.h>

#include "drivers/gpio_control.h"
#include "system/operating_mode.h"
#include "unity.h"

void test_operating_mode_defaults_and_invalid_storage_use_bridge(void)
{
    TEST_ASSERT_EQUAL_INT(OPERATING_MODE_BRIDGE, OPERATING_MODE_DEFAULT);
    TEST_ASSERT_EQUAL_INT(OPERATING_MODE_BRIDGE, operating_mode_from_persisted(0U));
    TEST_ASSERT_EQUAL_INT(OPERATING_MODE_BRIDGE, operating_mode_from_persisted(2U));
    TEST_ASSERT_EQUAL_INT(OPERATING_MODE_BRIDGE, operating_mode_from_persisted(UINT32_MAX));
}

void test_operating_mode_restores_gpio_from_storage(void)
{
    TEST_ASSERT_EQUAL_INT(OPERATING_MODE_GPIO, operating_mode_from_persisted(1U));
    TEST_ASSERT_EQUAL_STRING("bridge", operating_mode_name(OPERATING_MODE_BRIDGE));
    TEST_ASSERT_EQUAL_STRING("gpio", operating_mode_name(OPERATING_MODE_GPIO));
}

void test_operating_mode_change_requires_idle_pipeline(void)
{
    TEST_ASSERT_TRUE(operating_mode_change_allowed(OPERATING_MODE_BRIDGE,
                                                   OPERATING_MODE_BRIDGE, true, false));
    TEST_ASSERT_FALSE(operating_mode_change_allowed(OPERATING_MODE_BRIDGE,
                                                    OPERATING_MODE_GPIO, true, true));
    TEST_ASSERT_FALSE(operating_mode_change_allowed(OPERATING_MODE_BRIDGE,
                                                    OPERATING_MODE_GPIO, false, false));
    TEST_ASSERT_TRUE(operating_mode_change_allowed(OPERATING_MODE_BRIDGE,
                                                   OPERATING_MODE_GPIO, false, true));
}

void test_bridge_mode_rejects_every_manual_gpio_write(void)
{
    for (size_t i = 0U; i < gpio_control_signal_count(); i++)
    {
        const gpio_signal_info_t* info = gpio_control_signal_at(i);
        TEST_ASSERT_NOT_NULL(info);
        TEST_ASSERT_FALSE(gpio_control_manual_write_allowed(OPERATING_MODE_BRIDGE, info->signal));
    }
}

void test_gpio_mode_allows_only_nonreserved_outputs(void)
{
    const char* writable[] = {"txd", "rts", "dtr", "tx_active", "tck"};
    const char* rejected[] = {"rxd", "cts", "dsr", "dcd", "rck", "xck", "w550_reset"};

    for (size_t i = 0U; i < (sizeof(writable) / sizeof(writable[0])); i++)
    {
        const gpio_signal_info_t* info = gpio_control_find_signal(writable[i]);
        TEST_ASSERT_NOT_NULL(info);
        TEST_ASSERT_TRUE(gpio_control_manual_write_allowed(OPERATING_MODE_GPIO, info->signal));
    }

    for (size_t i = 0U; i < (sizeof(rejected) / sizeof(rejected[0])); i++)
    {
        const gpio_signal_info_t* info = gpio_control_find_signal(rejected[i]);
        TEST_ASSERT_NOT_NULL(info);
        TEST_ASSERT_FALSE(gpio_control_manual_write_allowed(OPERATING_MODE_GPIO, info->signal));
    }
}

void test_gpio_catalog_includes_external_clock_and_reserved_reset(void)
{
    const gpio_signal_info_t* xck = gpio_control_find_signal("xck");
    TEST_ASSERT_NOT_NULL(xck);
    TEST_ASSERT_EQUAL_INT(GPIO_SIGNAL_INPUT, xck->direction);
    TEST_ASSERT_FALSE(xck->reserved);

    const gpio_signal_info_t* reset = gpio_control_find_signal("w550_reset");
    TEST_ASSERT_NOT_NULL(reset);
    TEST_ASSERT_EQUAL_INT(GPIO_SIGNAL_OUTPUT, reset->direction);
    TEST_ASSERT_TRUE(reset->reserved);
}
