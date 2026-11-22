#include <stddef.h>
#include <string.h>

#include "drivers/gpio_control.h"
#include "system/management.h"
#include "system/management_catalog.h"
#include "unity.h"

void test_management_catalog_paths_are_unique(void)
{
    TEST_ASSERT_GREATER_THAN_UINT32(0U, management_catalog_count());
    for (size_t i = 0U; i < management_catalog_count(); i++)
    {
        const management_resource_descriptor_t* left = management_catalog_at(i);
        TEST_ASSERT_NOT_NULL(left);
        TEST_ASSERT_NOT_NULL(left->path);
        for (size_t j = i + 1U; j < management_catalog_count(); j++)
        {
            const management_resource_descriptor_t* right = management_catalog_at(j);
            TEST_ASSERT_NOT_NULL(right);
            TEST_ASSERT_NOT_EQUAL(0, strcmp(left->path, right->path));
        }
    }
}

void test_management_catalog_exposes_common_mode_and_status_resources(void)
{
    const management_resource_descriptor_t* status = management_catalog_find("status");
    TEST_ASSERT_NOT_NULL(status);
    TEST_ASSERT_BITS_HIGH(MANAGEMENT_METHOD_MASK(MANAGEMENT_METHOD_GET), status->method_mask);

    const management_resource_descriptor_t* mode = management_catalog_find("system.mode");
    TEST_ASSERT_NOT_NULL(mode);
    TEST_ASSERT_BITS_HIGH(MANAGEMENT_METHOD_MASK(MANAGEMENT_METHOD_GET), mode->method_mask);
    TEST_ASSERT_BITS_HIGH(MANAGEMENT_METHOD_MASK(MANAGEMENT_METHOD_SET), mode->method_mask);
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_VALUE_MODE, mode->value_kind);
}

void test_management_get_version_returns_runtime_version(void)
{
    const management_resource_descriptor_t* descriptor =
        management_catalog_find("system.version");
    TEST_ASSERT_NOT_NULL(descriptor);
    TEST_ASSERT_BITS_HIGH(MANAGEMENT_METHOD_MASK(MANAGEMENT_METHOD_GET),
                          descriptor->method_mask);
    TEST_ASSERT_BITS_LOW(MANAGEMENT_METHOD_MASK(MANAGEMENT_METHOD_SET),
                         descriptor->method_mask);
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_VALUE_TEXT, descriptor->value_kind);

    management_runtime_view_t runtime = {
        .firmware_version = "v9.8.7-test",
    };
    management_request_t request = {
        .method   = MANAGEMENT_METHOD_GET,
        .resource = MANAGEMENT_RESOURCE_SYSTEM_VERSION,
    };
    management_response_t response;

    TEST_ASSERT_EQUAL_INT(
        MANAGEMENT_RESULT_OK,
        management_execute(&request, &runtime, &response));
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_VALUE_TEXT, response.value.kind);
    TEST_ASSERT_EQUAL_STRING("v9.8.7-test", response.value.data.text);
}

void test_management_get_mode_returns_runtime_mode(void)
{
    const management_runtime_view_t runtime = {.operating_mode = OPERATING_MODE_GPIO,
                                               .queue_empty    = true};
    const management_request_t request = {.method   = MANAGEMENT_METHOD_GET,
                                          .resource = MANAGEMENT_RESOURCE_SYSTEM_MODE};
    management_response_t response;

    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_OK,
                          management_execute(&request, &runtime, &response));
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_VALUE_MODE, response.value.kind);
    TEST_ASSERT_EQUAL_INT(OPERATING_MODE_GPIO, response.value.data.mode);
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_EFFECT_NONE, response.effect.kind);
}

void test_management_set_mode_prepares_persisted_reboot_effect(void)
{
    const management_runtime_view_t runtime = {.operating_mode = OPERATING_MODE_BRIDGE,
                                               .queue_empty    = true};
    const management_request_t request = {
        .method   = MANAGEMENT_METHOD_SET,
        .resource = MANAGEMENT_RESOURCE_SYSTEM_MODE,
        .value    = {.kind = MANAGEMENT_VALUE_MODE, .data.mode = OPERATING_MODE_GPIO},
    };
    management_response_t response;

    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_ACCEPTED,
                          management_execute(&request, &runtime, &response));
    TEST_ASSERT_TRUE(response.changed);
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_EFFECT_SET, response.effect.kind);
    TEST_ASSERT_EQUAL_INT(OPERATING_MODE_GPIO, response.effect.value.data.mode);
    TEST_ASSERT_TRUE(response.effect.persist);
    TEST_ASSERT_TRUE(response.effect.reboot);
}

void test_management_set_mode_rejects_busy_pipeline(void)
{
    const management_runtime_view_t runtime = {.operating_mode = OPERATING_MODE_BRIDGE,
                                               .frame_active   = true,
                                               .queue_empty    = true};
    const management_request_t request = {
        .method   = MANAGEMENT_METHOD_SET,
        .resource = MANAGEMENT_RESOURCE_SYSTEM_MODE,
        .value    = {.kind = MANAGEMENT_VALUE_MODE, .data.mode = OPERATING_MODE_GPIO},
    };
    management_response_t response;

    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_CONFLICT,
                          management_execute(&request, &runtime, &response));
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_EFFECT_NONE, response.effect.kind);
}

void test_management_set_mode_is_idempotent(void)
{
    const management_runtime_view_t runtime = {.operating_mode = OPERATING_MODE_GPIO,
                                               .frame_active   = true,
                                               .queue_empty    = false};
    const management_request_t request = {
        .method   = MANAGEMENT_METHOD_SET,
        .resource = MANAGEMENT_RESOURCE_SYSTEM_MODE,
        .value    = {.kind = MANAGEMENT_VALUE_MODE, .data.mode = OPERATING_MODE_GPIO},
    };
    management_response_t response;

    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_OK,
                          management_execute(&request, &runtime, &response));
    TEST_ASSERT_FALSE(response.changed);
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_EFFECT_NONE, response.effect.kind);
}

void test_management_gpio_write_uses_common_mode_policy(void)
{
    const gpio_signal_info_t* rts = gpio_control_find_signal("rts");
    TEST_ASSERT_NOT_NULL(rts);
    management_request_t request = {
        .method   = MANAGEMENT_METHOD_SET,
        .resource = MANAGEMENT_RESOURCE_GPIO,
        .value = {.kind = MANAGEMENT_VALUE_GPIO,
                  .data.gpio = {.signal = rts->signal, .level = true}},
    };
    management_response_t response;
    management_runtime_view_t runtime = {.operating_mode = OPERATING_MODE_BRIDGE,
                                         .queue_empty    = true};

    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_CONFLICT,
                          management_execute(&request, &runtime, &response));

    runtime.operating_mode = OPERATING_MODE_GPIO;
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_OK,
                          management_execute(&request, &runtime, &response));
    TEST_ASSERT_TRUE(response.changed);
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_EFFECT_SET, response.effect.kind);
}

void test_management_gpio_write_rejects_input_and_reserved_signals(void)
{
    const char* rejected[] = {"cts", "w550_reset"};
    const management_runtime_view_t runtime = {.operating_mode = OPERATING_MODE_GPIO,
                                               .queue_empty    = true};

    for (size_t i = 0U; i < (sizeof(rejected) / sizeof(rejected[0])); i++)
    {
        const gpio_signal_info_t* info = gpio_control_find_signal(rejected[i]);
        TEST_ASSERT_NOT_NULL(info);
        const management_request_t request = {
            .method   = MANAGEMENT_METHOD_SET,
            .resource = MANAGEMENT_RESOURCE_GPIO,
            .value = {.kind = MANAGEMENT_VALUE_GPIO,
                      .data.gpio = {.signal = info->signal, .level = true}},
        };
        management_response_t response;
        TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_READ_ONLY,
                              management_execute(&request, &runtime, &response));
    }
}

void test_management_local_ip_is_accepted_without_forcing_reboot(void)
{
    const management_runtime_view_t runtime = {
        .local_ip = {.address = {192U, 168U, 29U, 20U}, .prefix_length = 24U},
    };
    const management_request_t request = {
        .method = MANAGEMENT_METHOD_SET,
        .resource = MANAGEMENT_RESOURCE_NET_IP_LOCAL,
        .value = {
            .kind = MANAGEMENT_VALUE_IPV4_CIDR,
            .data.ipv4_cidr = {
                .address = {192U, 168U, 29U, 21U},
                .prefix_length = 24U,
            },
        },
    };
    management_response_t response;

    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_ACCEPTED,
                          management_execute(&request, &runtime, &response));
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_EFFECT_SET, response.effect.kind);
    TEST_ASSERT_TRUE(response.effect.persist);
    TEST_ASSERT_FALSE(response.effect.reboot);
}

void test_management_reset_stats_prepares_runtime_effect(void)
{
    const management_resource_descriptor_t* descriptor =
        management_catalog_find("actions.reset-stats");
    TEST_ASSERT_NOT_NULL(descriptor);
    TEST_ASSERT_BITS_HIGH(MANAGEMENT_METHOD_MASK(MANAGEMENT_METHOD_ACTION),
                          descriptor->method_mask);

    const management_runtime_view_t runtime = {0};
    const management_request_t request = {
        .method   = MANAGEMENT_METHOD_ACTION,
        .resource = MANAGEMENT_RESOURCE_ACTION_RESET_STATS,
    };
    management_response_t response;

    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_OK,
                          management_execute(&request, &runtime, &response));
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_EFFECT_RESET_STATS, response.effect.kind);
    TEST_ASSERT_FALSE(response.effect.persist);
    TEST_ASSERT_FALSE(response.effect.reboot);
}
