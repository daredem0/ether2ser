#include <stddef.h>
#include <stdint.h>
#include <string.h>

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

void test_parse_set_ip_args_plain_ok(void)
{
    uint8_t ip[4]   = {0};
    uint8_t mask[4] = {0};
    TEST_ASSERT_EQUAL(E2S_OK, parse_set_ip_args("192.168.10.5/24", ip, mask));
    TEST_ASSERT_EQUAL_UINT8(192, ip[0]);
    TEST_ASSERT_EQUAL_UINT8(168, ip[1]);
    TEST_ASSERT_EQUAL_UINT8(10, ip[2]);
    TEST_ASSERT_EQUAL_UINT8(5, ip[3]);
    TEST_ASSERT_EQUAL_UINT8(255, mask[0]);
    TEST_ASSERT_EQUAL_UINT8(255, mask[1]);
    TEST_ASSERT_EQUAL_UINT8(255, mask[2]);
    TEST_ASSERT_EQUAL_UINT8(0, mask[3]);
}

void test_parse_set_ip_args_with_ip_prefix_ok(void)
{
    uint8_t ip[4]   = {0};
    uint8_t mask[4] = {0};
    TEST_ASSERT_EQUAL(E2S_OK, parse_set_ip_args("ip 10.0.0.9/8", ip, mask));
    TEST_ASSERT_EQUAL_UINT8(10, ip[0]);
    TEST_ASSERT_EQUAL_UINT8(0, ip[1]);
    TEST_ASSERT_EQUAL_UINT8(0, ip[2]);
    TEST_ASSERT_EQUAL_UINT8(9, ip[3]);
    TEST_ASSERT_EQUAL_UINT8(255, mask[0]);
    TEST_ASSERT_EQUAL_UINT8(0, mask[1]);
    TEST_ASSERT_EQUAL_UINT8(0, mask[2]);
    TEST_ASSERT_EQUAL_UINT8(0, mask[3]);
}

void test_parse_set_ip_args_invalid_values_fail(void)
{
    uint8_t ip[4]   = {0};
    uint8_t mask[4] = {0};
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_SET, parse_set_ip_args("256.1.2.3/24", ip, mask));
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_SET, parse_set_ip_args("1.2.3.4/33", ip, mask));
}

void test_parse_set_net_ip_args_ok_and_prefix_required(void)
{
    uint8_t ip[4]   = {0};
    uint8_t mask[4] = {0};
    TEST_ASSERT_EQUAL(E2S_OK, parse_set_net_ip_args("net 172.16.0.1/16", ip, mask));
    TEST_ASSERT_EQUAL_UINT8(172, ip[0]);
    TEST_ASSERT_EQUAL_UINT8(16, ip[1]);
    TEST_ASSERT_EQUAL_UINT8(0, ip[2]);
    TEST_ASSERT_EQUAL_UINT8(1, ip[3]);
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_SET, parse_set_net_ip_args("172.16.0.1/16", ip, mask));
}

void test_parse_set_ip_remote_args_ok_and_fail(void)
{
    uint8_t ip[4] = {0};
    TEST_ASSERT_EQUAL(E2S_OK, parse_set_ip_remote_args("192.168.29.5", ip));
    TEST_ASSERT_EQUAL_UINT8(192, ip[0]);
    TEST_ASSERT_EQUAL(E2S_OK, parse_set_ip_remote_args("ip 192.168.29.6", ip));
    TEST_ASSERT_EQUAL_UINT8(6, ip[3]);
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_SET, parse_set_ip_remote_args("ip 192.168.29", ip));
}

void test_parse_set_gateway_args_ok_and_fail(void)
{
    uint8_t ip[4] = {0};
    TEST_ASSERT_EQUAL(E2S_OK, parse_set_gateway_args("gateway 192.168.29.1", ip));
    TEST_ASSERT_EQUAL_UINT8(1, ip[3]);
    TEST_ASSERT_EQUAL(E2S_OK, parse_set_gateway_args("192.168.29.254", ip));
    TEST_ASSERT_EQUAL_UINT8(254, ip[3]);
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_SET, parse_set_gateway_args("gateway 999.1.1.1", ip));
}

void test_parse_set_udp_port_local_args_ok_and_fail(void)
{
    uint16_t port = 0;
    TEST_ASSERT_EQUAL(E2S_OK, parse_set_udp_port_local_args("6969", &port));
    TEST_ASSERT_EQUAL_UINT16(6969, port);
    TEST_ASSERT_EQUAL(E2S_OK, parse_set_udp_port_local_args("port 1234", &port));
    TEST_ASSERT_EQUAL_UINT16(1234, port);
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_SET, parse_set_udp_port_local_args("70000", &port));
}

void test_parse_set_udp_port_remote_args_ok_and_fail(void)
{
    uint16_t port = 0;
    TEST_ASSERT_EQUAL(E2S_OK, parse_set_udp_port_remote_args("port 9999", &port));
    TEST_ASSERT_EQUAL_UINT16(9999, port);
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_SET, parse_set_udp_port_remote_args("port x", &port));
}

void test_parse_set_v24_polarities_empty_or_null_args_ok(void)
{
    V24_POLARITIES_T p;
    memset(&p, 0xAA, sizeof(p));
    TEST_ASSERT_EQUAL(E2S_OK, parse_set_v24_polarities(NULL, &p));
    TEST_ASSERT_FALSE(p.tx_polarities.txd_inverted);
    TEST_ASSERT_FALSE(p.rx_polarities.rxd_inverted);

    memset(&p, 0xAA, sizeof(p));
    TEST_ASSERT_EQUAL(E2S_OK, parse_set_v24_polarities("   ", &p));
    TEST_ASSERT_FALSE(p.tx_polarities.rts_inverted);
    TEST_ASSERT_FALSE(p.rx_polarities.dcd_inverted);
}

void test_parse_set_v24_polarities_tokens_ok(void)
{
    V24_POLARITIES_T p = {0};
    TEST_ASSERT_EQUAL(E2S_OK, parse_set_v24_polarities("invert txd, rxd,cts,dtr,dcd,rts", &p));
    TEST_ASSERT_TRUE(p.tx_polarities.txd_inverted);
    TEST_ASSERT_TRUE(p.tx_polarities.rts_inverted);
    TEST_ASSERT_TRUE(p.tx_polarities.cts_inverted);
    TEST_ASSERT_TRUE(p.tx_polarities.dtr_inverted);
    TEST_ASSERT_TRUE(p.rx_polarities.rxd_inverted);
    TEST_ASSERT_TRUE(p.rx_polarities.dcd_inverted);
}

void test_parse_set_v24_polarities_invalid_token_fails(void)
{
    V24_POLARITIES_T p = {0};
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_SET, parse_set_v24_polarities("invert dsr", &p));
    TEST_ASSERT_EQUAL(E2S_OK, parse_set_v24_polarities("invert txd,,rxd", &p));
    TEST_ASSERT_TRUE(p.tx_polarities.txd_inverted);
    TEST_ASSERT_TRUE(p.rx_polarities.rxd_inverted);
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_SET, parse_set_v24_polarities("invert unknown", &p));
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_SET, parse_set_v24_polarities("invert this_token_name_is_way_too_long_for_the_parser_buffer", &p));
}

void test_parse_set_v24_polarities_null_output_fails(void)
{
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_SET, parse_set_v24_polarities("invert txd", NULL));
}

void test_parse_set_v24_baudrate_valid_and_invalid(void)
{
    V24_BAUDRATE_T baud = 0;
    TEST_ASSERT_EQUAL(E2S_OK, parse_set_v24_baudrate("9600", &baud));
    TEST_ASSERT_EQUAL(V24_BAUD_9600, baud);
    TEST_ASSERT_EQUAL(E2S_OK, parse_set_v24_baudrate(" 16000", &baud));
    TEST_ASSERT_EQUAL(V24_BAUD_16000, baud);
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_SET, parse_set_v24_baudrate("12345", &baud));
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_SET, parse_set_v24_baudrate("", &baud));
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_SET, parse_set_v24_baudrate(NULL, &baud));
    TEST_ASSERT_EQUAL(E2S_ERR_CLI_USAGE_SET, parse_set_v24_baudrate("9600", NULL));
}
