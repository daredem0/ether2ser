#include <string.h>

#include "drivers/gpio_control.h"
#include "system/management_http.h"
#include "unity.h"

static management_http_parse_result_t parse_text(const char* text,
                                                  management_request_t* request,
                                                  const char** path)
{
    return management_http_parse_request((const uint8_t*)text, strlen(text), request, path);
}

void test_management_http_maps_get_and_nested_paths(void)
{
    management_request_t request;
    const char* path = NULL;
    TEST_ASSERT_EQUAL_INT(
        MANAGEMENT_HTTP_PARSE_READY,
        parse_text("GET /api/v1/status HTTP/1.1\r\nHost: device\r\n\r\n", &request, &path));
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_METHOD_GET, request.method);
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESOURCE_STATUS, request.resource);
    TEST_ASSERT_EQUAL_STRING("status", path);

    TEST_ASSERT_EQUAL_INT(
        MANAGEMENT_HTTP_PARSE_READY,
        parse_text("GET /api/v1/pins/rxd HTTP/1.1\r\n\r\n", &request, &path));
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESOURCE_PIN, request.resource);
    TEST_ASSERT_EQUAL_STRING("pin", path);
}

void test_management_http_maps_put_values_through_common_request_semantics(void)
{
    management_request_t request;
    const char* path = NULL;
    const char* mode =
        "PUT /api/v1/system/mode HTTP/1.1\r\nContent-Length: 16\r\n\r\n"
        "{\"value\":\"gpio\"}";
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_HTTP_PARSE_READY, parse_text(mode, &request, &path));
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_METHOD_SET, request.method);
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESOURCE_SYSTEM_MODE, request.resource);
    TEST_ASSERT_EQUAL_INT(OPERATING_MODE_GPIO, request.value.data.mode);
    TEST_ASSERT_EQUAL_STRING("system.mode", path);

    const char* baud =
        "PUT /api/v1/v24/baudrate HTTP/1.1\r\nContent-Length: 16\r\n\r\n"
        "{\"value\":115200}";
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_HTTP_PARSE_READY, parse_text(baud, &request, &path));
    TEST_ASSERT_EQUAL_UINT32(115200U, request.value.data.u32);

    const char* gpio =
        "PUT /api/v1/gpio/rts HTTP/1.1\r\nContent-Length: 11\r\n\r\n"
        "{\"value\":1}";
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_HTTP_PARSE_READY, parse_text(gpio, &request, &path));
    const gpio_signal_info_t* rts = gpio_control_find_signal("rts");
    TEST_ASSERT_NOT_NULL(rts);
    TEST_ASSERT_EQUAL_INT(rts->signal, request.value.data.gpio.signal);
    TEST_ASSERT_TRUE(request.value.data.gpio.level);
}

void test_management_http_maps_actions_and_enforces_wipe_confirmation(void)
{
    management_request_t request;
    const char* path = NULL;
    TEST_ASSERT_EQUAL_INT(
        MANAGEMENT_HTTP_PARSE_READY,
        parse_text("POST /api/v1/actions/save HTTP/1.1\r\nContent-Length: 0\r\n\r\n",
                   &request, &path));
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESOURCE_ACTION_SAVE, request.resource);

    const char* wipe =
        "POST /api/v1/actions/wipe HTTP/1.1\r\nContent-Length: 18\r\n\r\n"
        "{\"confirm\":\"wipe\"}";
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_HTTP_PARSE_READY, parse_text(wipe, &request, &path));
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_RESOURCE_ACTION_WIPE, request.resource);
    TEST_ASSERT_EQUAL_STRING("wipe", request.value.data.text);

    TEST_ASSERT_EQUAL_INT(
        MANAGEMENT_HTTP_PARSE_BAD_REQUEST,
        parse_text("POST /api/v1/actions/wipe HTTP/1.1\r\nContent-Length: 0\r\n\r\n",
                   &request, &path));
}

void test_management_http_rejects_incomplete_oversized_and_unsupported_requests(void)
{
    management_request_t request;
    const char* path = NULL;
    TEST_ASSERT_EQUAL_INT(
        MANAGEMENT_HTTP_PARSE_INCOMPLETE,
        parse_text("GET /api/v1/status HTTP/1.1\r\nHost: device\r\n", &request, &path));
    TEST_ASSERT_EQUAL_INT(
        MANAGEMENT_HTTP_PARSE_METHOD_NOT_ALLOWED,
        parse_text("DELETE /api/v1/status HTTP/1.1\r\n\r\n", &request, &path));
    TEST_ASSERT_EQUAL_INT(
        MANAGEMENT_HTTP_PARSE_BAD_REQUEST,
        parse_text("GET /other/status HTTP/1.1\r\n\r\n", &request, &path));
    TEST_ASSERT_EQUAL_INT(
        MANAGEMENT_HTTP_PARSE_NOT_FOUND,
        parse_text("GET /api/v1/missing HTTP/1.1\r\n\r\n", &request, &path));

    uint8_t oversized[MANAGEMENT_HTTP_REQUEST_CAPACITY + 1U];
    memset(oversized, 'x', sizeof(oversized));
    TEST_ASSERT_EQUAL_INT(
        MANAGEMENT_HTTP_PARSE_TOO_LARGE,
        management_http_parse_request(oversized, sizeof(oversized), &request, &path));
}

void test_management_http_formats_bounded_json_success_and_error(void)
{
    management_response_t response = {
        .result = MANAGEMENT_RESULT_OK,
        .value = {.kind = MANAGEMENT_VALUE_MODE, .data.mode = OPERATING_MODE_GPIO},
    };
    char output[512];
    size_t length = 0U;
    TEST_ASSERT_TRUE(management_http_format_response("system.mode", &response, output,
                                                    sizeof(output), &length));
    TEST_ASSERT_EQUAL_UINT(strlen(output), length);
    TEST_ASSERT_NOT_NULL(strstr(output, "HTTP/1.1 200 OK\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(output, "Content-Type: application/json\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(output, "Connection: close\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(output,
        "{\"result\":\"ok\",\"path\":\"system.mode\",\"value\":\"gpio\"}"));

    response.result = MANAGEMENT_RESULT_CONFLICT;
    response.value.kind = MANAGEMENT_VALUE_NONE;
    TEST_ASSERT_TRUE(management_http_format_response("gpio", &response, output,
                                                    sizeof(output), &length));
    TEST_ASSERT_NOT_NULL(strstr(output, "HTTP/1.1 409 Conflict\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(output,
        "{\"result\":\"error\",\"error\":\"conflict\"}"));

    char tiny[16];
    TEST_ASSERT_FALSE(management_http_format_response("gpio", &response, tiny,
                                                     sizeof(tiny), &length));

}
void test_management_http_formats_shared_status_and_config_snapshots(void)
{
    management_response_t response;
    memset(&response, 0, sizeof(response));
    response.result = MANAGEMENT_RESULT_OK;
    response.snapshot.operating_mode = OPERATING_MODE_BRIDGE;
    response.snapshot.status.rates.dma_baudrate_hz = 115200U;
    response.snapshot.status.stats.udp_rx_frames = 42U;
    response.snapshot.status.tx_fifo_underruns = 3U;
    response.snapshot.local_ip =
        (management_ipv4_cidr_t){.address = {192U, 168U, 29U, 20U},
                                 .prefix_length = 24U};
    response.snapshot.remote_ip[0] = 192U;
    response.snapshot.remote_ip[1] = 168U;
    response.snapshot.remote_ip[2] = 29U;
    response.snapshot.remote_ip[3] = 21U;
    response.snapshot.local_udp_port = 6969U;
    response.snapshot.v24_baudrate = 16000U;
    response.snapshot.memory.ram_free = 12345U;

    char output[MANAGEMENT_HTTP_RESPONSE_CAPACITY];
    size_t length = 0U;
    TEST_ASSERT_TRUE(management_http_format_response("status", &response, output,
                                                    sizeof(output), &length));
    TEST_ASSERT_NOT_NULL(strstr(output, "\"dma_baudrate_hz\":115200"));
    TEST_ASSERT_NOT_NULL(strstr(output, "\"udp_rx_frames\":42"));
    TEST_ASSERT_NOT_NULL(strstr(output, "\"tx_fifo_underruns\":3"));

    TEST_ASSERT_TRUE(management_http_format_response("config", &response, output,
                                                    sizeof(output), &length));
    TEST_ASSERT_NOT_NULL(strstr(output, "\"local\":\"192.168.29.20/24\""));
    TEST_ASSERT_NOT_NULL(strstr(output, "\"baudrate\":16000"));

    TEST_ASSERT_TRUE(management_http_format_response("memory", &response, output,
                                                    sizeof(output), &length));
    TEST_ASSERT_NOT_NULL(strstr(output, "\"ram_free\":12345"));
}
