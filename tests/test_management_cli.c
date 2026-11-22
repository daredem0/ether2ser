#include <string.h>

#include "drivers/gpio_control.h"
#include "system/management_cli.h"
#include "unity.h"

void test_management_cli_parses_get_status(void)
{
    management_request_t request;
    const char* path = NULL;
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_OK,
                          management_cli_parse("get status", &request, &path));
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_METHOD_GET, request.method);
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESOURCE_STATUS, request.resource);
    TEST_ASSERT_EQUAL_STRING("status", path);
}

void test_management_cli_parses_semantic_mode_and_clock_values(void)
{
    management_request_t request;
    const char* path = NULL;
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_OK,
                          management_cli_parse("set system.mode gpio", &request, &path));
    TEST_ASSERT_EQUAL_INT(OPERATING_MODE_GPIO, request.value.data.mode);

    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_OK,
                          management_cli_parse("set v24.txclock external", &request, &path));
    TEST_ASSERT_TRUE(request.value.data.boolean);
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_INVALID_VALUE,
                          management_cli_parse("set v24.txclock 1", &request, &path));
}

void test_management_cli_parses_gpio_path_and_level(void)
{
    management_request_t request;
    const char* path = NULL;
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_OK,
                          management_cli_parse("set gpio.rts 1", &request, &path));
    const gpio_signal_info_t* rts = gpio_control_find_signal("rts");
    TEST_ASSERT_NOT_NULL(rts);
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESOURCE_GPIO, request.resource);
    TEST_ASSERT_EQUAL_INT(rts->signal, request.value.data.gpio.signal);
    TEST_ASSERT_TRUE(request.value.data.gpio.level);
    TEST_ASSERT_EQUAL_STRING("gpio", path);
}

void test_management_cli_parses_actions_and_wipe_confirmation(void)
{
    management_request_t request;
    const char* path = NULL;
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_OK,
                          management_cli_parse("save", &request, &path));
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_METHOD_ACTION, request.method);
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESOURCE_ACTION_SAVE, request.resource);

    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_OK,
                          management_cli_parse("wipe wipe", &request, &path));
    TEST_ASSERT_EQUAL_STRING("wipe", request.value.data.text);
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_INVALID_VALUE,
                          management_cli_parse("wipe", &request, &path));
}

void test_management_cli_rejects_unknown_trailing_and_oversized_lines(void)
{
    management_request_t request;
    const char* path = NULL;
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_NOT_FOUND,
                          management_cli_parse("get missing", &request, &path));
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_INVALID_VALUE,
                          management_cli_parse("get status trailing", &request, &path));

    char oversized[MANAGEMENT_CLI_MAX_LINE + 2U];
    memset(oversized, 'a', sizeof(oversized));
    oversized[sizeof(oversized) - 1U] = '\0';
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_TOO_LARGE,
                          management_cli_parse(oversized, &request, &path));
}

void test_management_cli_human_commands_keep_original_shortcuts(void)
{
    management_request_t request;
    const char* path = NULL;

    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_OK,
                          management_cli_parse_human("status", &request, &path));
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESOURCE_STATUS, request.resource);
    TEST_ASSERT_EQUAL_STRING("status", path);

    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_OK,
                          management_cli_parse_human("mem", &request, &path));
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESOURCE_MEMORY, request.resource);

    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_OK,
                          management_cli_parse_human("mode gpio", &request, &path));
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_METHOD_SET, request.method);
    TEST_ASSERT_EQUAL_INT(OPERATING_MODE_GPIO, request.value.data.mode);

    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESULT_OK,
                          management_cli_parse_human("pininfo rxd", &request, &path));
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESOURCE_PIN, request.resource);
}

void test_management_cli_human_commands_accept_original_category_syntax(void)
{
    management_request_t request;
    const char* path = NULL;

    TEST_ASSERT_EQUAL_INT(
        MANAGEMENT_RESULT_OK,
        management_cli_parse_human("set net ip.local 192.168.29.20/24",
                                   &request, &path));
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESOURCE_NET_IP_LOCAL, request.resource);

    TEST_ASSERT_EQUAL_INT(
        MANAGEMENT_RESULT_OK,
        management_cli_parse_human("set v24 txclock 1", &request, &path));
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESOURCE_V24_TXCLOCK, request.resource);
    TEST_ASSERT_TRUE(request.value.data.boolean);

    TEST_ASSERT_EQUAL_INT(
        MANAGEMENT_RESULT_OK,
        management_cli_parse_human("get gpio rts", &request, &path));
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESOURCE_GPIO, request.resource);

    TEST_ASSERT_EQUAL_INT(
        MANAGEMENT_RESULT_OK,
        management_cli_parse_human("set loglevel debug", &request, &path));
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESOURCE_SYSTEM_LOGLEVEL, request.resource);
}

void test_management_cli_formats_framed_success_and_error(void)
{
    management_response_t response = {
        .result = MANAGEMENT_RESULT_OK,
        .value  = {.kind = MANAGEMENT_VALUE_MODE, .data.mode = OPERATING_MODE_GPIO},
    };
    char output[256];
    TEST_ASSERT_TRUE(management_cli_format_response(7U, "system.mode", &response, output,
                                                    sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "@response 7 begin system.mode\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(output, "system.mode=gpio\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(output, "@response 7 end ok\r\n"));

    response.result = MANAGEMENT_RESULT_CONFLICT;
    response.value.kind = MANAGEMENT_VALUE_NONE;
    TEST_ASSERT_TRUE(management_cli_format_response(8U, "gpio", &response, output,
                                                    sizeof(output)));
    TEST_ASSERT_EQUAL_STRING("@response 8 end error conflict\r\n", output);
}

void test_management_cli_help_is_human_readable(void)
{
    const char* help = management_cli_help_overview();
    TEST_ASSERT_NOT_NULL(help);
    TEST_ASSERT_NOT_NULL(strstr(help, "Read-only views:\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(help, "  status"));
    TEST_ASSERT_NOT_NULL(strstr(help, "  mode <bridge|gpio>"));
    TEST_ASSERT_NOT_NULL(strstr(help, "Actions:\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(help, "Pins:\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(help, "Baudrates:\r\n"));
    TEST_ASSERT_NULL(strstr(help, "methods=0x"));
    TEST_ASSERT_NULL(strstr(help, "type="));
}

void test_management_cli_help_provides_targeted_usage(void)
{
    TEST_ASSERT_EQUAL_STRING(
        "  mode\r\n"
        "  mode <bridge|gpio>\r\n",
        management_cli_help_usage(MANAGEMENT_RESOURCE_SYSTEM_MODE));
    TEST_ASSERT_EQUAL_STRING(
        "  get gpio <signal>\r\n"
        "  set gpio <signal> <0|1>  (gpio mode, outputs only)\r\n",
        management_cli_help_usage(MANAGEMENT_RESOURCE_GPIO));
}
