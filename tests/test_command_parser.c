#include <stddef.h>
#include <stdint.h>

#include "system/cli_commands.h"
#include "system/cli_parser.h"
#include "system/error.h"
#include "unity.h"

void test_cli_parse_command_only(void)
{
    char cmd[16], args[64];
    TEST_ASSERT_EQUAL(E2S_OK, cli_parse("help", cmd, sizeof(cmd), args, sizeof(args)));
    TEST_ASSERT_EQUAL_STRING("help", cmd);
    TEST_ASSERT_EQUAL_STRING("", args);
}

void test_cli_parse_command_and_args(void)
{
    char cmd[16], args[64];
    TEST_ASSERT_EQUAL(E2S_OK, cli_parse("set gpio txd 1", cmd, sizeof(cmd), args, sizeof(args)));
    TEST_ASSERT_EQUAL_STRING("set", cmd);
    TEST_ASSERT_EQUAL_STRING("gpio txd 1", args);
}

void test_cli_parse_empty_line(void)
{
    char cmd[16], args[64];
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_EMPTY_LINE, cli_parse("", cmd, sizeof(cmd), args, sizeof(args)));
}

void test_cli_parse_space_only_line(void)
{
    char cmd[16], args[64];
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_EMPTY_LINE,
                      cli_parse("      ", cmd, sizeof(cmd), args, sizeof(args)));
}

void test_cli_parse_leading_spaces(void)
{
    char cmd[16], args[64];
    TEST_ASSERT_EQUAL(E2S_OK,
                      cli_parse("   set gpio txd 1", cmd, sizeof(cmd), args, sizeof(args)));
    TEST_ASSERT_EQUAL_STRING("set", cmd);
    TEST_ASSERT_EQUAL_STRING("gpio txd 1", args);
}

void test_cli_parse_cmd_truncation(void)
{
    char cmd[4], args[64];
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_LINE_TRUNCATED,
                      cli_parse("hello arg", cmd, sizeof(cmd), args, sizeof(args)));
}

void test_cli_parse_args_truncation(void)
{
    char cmd[16], args[5];
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_LINE_TRUNCATED,
                      cli_parse("set gpio txd 1", cmd, sizeof(cmd), args, sizeof(args)));
}

void test_cli_parse_exact_capacity_cmd_and_args(void)
{
    char cmd[4], args[7];
    TEST_ASSERT_EQUAL(E2S_OK, cli_parse("set abcdef", cmd, sizeof(cmd), args, sizeof(args)));
    TEST_ASSERT_EQUAL_STRING("set", cmd);
    TEST_ASSERT_EQUAL_STRING("abcdef", args);
}

void test_cli_parse_null_params(void)
{
    char cmd[16], args[64];
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_SET,
                      cli_parse(NULL, cmd, sizeof(cmd), args, sizeof(args)));
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_SET,
                      cli_parse("help", NULL, sizeof(cmd), args, sizeof(args)));
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_SET,
                      cli_parse("help", cmd, sizeof(cmd), NULL, sizeof(args)));
}

void test_cli_parse_zero_capacity(void)
{
    char cmd[16], args[64];
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_SET, cli_parse("help", cmd, 0, args, sizeof(args)));
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_SET, cli_parse("help", cmd, sizeof(cmd), args, 0));
}

void test_cli_parse_set_args_unknown_pin(void)
{
    const char*       args = "nonexistingpin 1";
    char              pin_name[16];
    int               value = 0;
    const pin_info_t* pin   = NULL;
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_UNKNOWN_PIN, parse_set_gpio_args(args, pin_name, &value, &pin));
}

void test_cli_parse_set_args_cli_usage_set(void)
{
    const char*       args = "txd";
    char              pin_name[16];
    int               value = 0;
    const pin_info_t* pin   = NULL;
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_SET, parse_set_gpio_args(args, pin_name, &value, &pin));
}

void test_cli_parse_set_args_input_only(void)
{
    const char*       args = "rxd 1";
    char              pin_name[16];
    int               value = 0;
    const pin_info_t* pin   = NULL;
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_PIN_INPUT_ONLY,
                      parse_set_gpio_args(args, pin_name, &value, &pin));
}

void test_cli_parse_get_args_cli_usage_get(void)
{
    const char*       args = "";
    char              pin_name[16];
    const pin_info_t* pin = NULL;
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_GET, parse_get_args(args, pin_name, &pin));
}

void test_cli_parse_get_args_cli_unknown_pin(void)
{
    const char*       args = "nonexistingpin";
    char              pin_name[16];
    const pin_info_t* pin = NULL;
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_UNKNOWN_PIN, parse_get_args(args, pin_name, &pin));
}

void test_cli_parse_set_args_cli_ok(void)
{
    const char*       args = "txd 1";
    char              pin_name[16];
    int               value = 0;
    const pin_info_t* pin   = NULL;
    TEST_ASSERT_EQUAL(E2S_OK, parse_set_gpio_args(args, pin_name, &value, &pin));
    TEST_ASSERT_EQUAL_STRING("txd", pin_name);
    TEST_ASSERT_EQUAL_INT(1, value);
}

void test_cli_parse_get_args_cli_ok(void)
{
    const char*       args = "txd";
    char              pin_name[16];
    const pin_info_t* pin = NULL;
    TEST_ASSERT_EQUAL(E2S_OK, parse_get_args(args, pin_name, &pin));
    TEST_ASSERT_EQUAL_STRING("txd", pin_name);
}

void test_cli_parse_set_args_invalid_value_upper_bound(void)
{
    const char*       args = "txd 2";
    char              pin_name[16];
    int               value = 0;
    const pin_info_t* pin   = NULL;
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_SET, parse_set_gpio_args(args, pin_name, &value, &pin));
}

void test_cli_parse_set_args_invalid_value_lower_bound(void)
{
    const char*       args = "txd -1";
    char              pin_name[16];
    int               value = 0;
    const pin_info_t* pin   = NULL;
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_SET, parse_set_gpio_args(args, pin_name, &value, &pin));
}

void test_cli_parse_set_args_extra_space_at_end(void)
{
    const char*       args = "txd 1 ";
    char              pin_name[16];
    int               value = 0;
    const pin_info_t* pin   = NULL;
    TEST_ASSERT_EQUAL(E2S_OK, parse_set_gpio_args(args, pin_name, &value, &pin));
    TEST_ASSERT_EQUAL_STRING("txd", pin_name);
    TEST_ASSERT_EQUAL_INT(1, value);
}

void test_cli_parse_set_args_extra_space_between_pin_and_args(void)
{
    const char*       args = "txd  1";
    char              pin_name[16];
    int               value = 0;
    const pin_info_t* pin   = NULL;
    TEST_ASSERT_EQUAL(E2S_OK, parse_set_gpio_args(args, pin_name, &value, &pin));
    TEST_ASSERT_EQUAL_STRING("txd", pin_name);
    TEST_ASSERT_EQUAL_INT(1, value);
}
