#include <string.h>

#include "socket.h"
#include "system/management_http_server.h"
#include "unity.h"

#define TEST_SEND_CHUNK 17U

typedef struct
{
    uint8_t status;
    const uint8_t* receive_data;
    size_t receive_length;
    size_t receive_position;
    size_t receive_limit;
    size_t sent_length;
    management_http_send_status_t send_status;
    unsigned int open_calls;
    unsigned int listen_calls;
    unsigned int receive_calls;
    unsigned int send_calls;
    unsigned int disconnect_calls;
    unsigned int execute_calls;
    unsigned int apply_calls;
    uint8_t open_socket;
    uint8_t open_protocol;
    uint16_t open_port;
    uint8_t open_flags;
} http_fixture_t;

static http_fixture_t fixture;

static uint8_t fake_status(uint8_t socket_number)
{
    (void)socket_number;
    return fixture.status;
}

static int8_t fake_open(uint8_t socket_number, uint8_t protocol, uint16_t port,
                        uint8_t flags)
{
    fixture.open_calls++;
    fixture.open_socket = socket_number;
    fixture.open_protocol = protocol;
    fixture.open_port = port;
    fixture.open_flags = flags;
    return (int8_t)socket_number;
}

static int8_t fake_listen(uint8_t socket_number)
{
    (void)socket_number;
    fixture.listen_calls++;
    return SOCK_OK;
}

static int8_t fake_disconnect(uint8_t socket_number)
{
    (void)socket_number;
    fixture.disconnect_calls++;
    return SOCK_OK;
}

static int8_t fake_close(uint8_t socket_number)
{
    (void)socket_number;
    return SOCK_OK;
}

static uint16_t fake_rx_available(uint8_t socket_number)
{
    (void)socket_number;
    return (uint16_t)(fixture.receive_length - fixture.receive_position);
}

static uint16_t fake_tx_available(uint8_t socket_number)
{
    (void)socket_number;
    return TEST_SEND_CHUNK;
}

static int32_t fake_receive(uint8_t socket_number, uint8_t* data, uint16_t length)
{
    (void)socket_number;
    fixture.receive_calls++;
    size_t remaining = fixture.receive_length - fixture.receive_position;
    size_t amount = length;
    if (amount > remaining)
    {
        amount = remaining;
    }
    if ((fixture.receive_limit > 0U) && (amount > fixture.receive_limit))
    {
        amount = fixture.receive_limit;
    }
    memcpy(data, &fixture.receive_data[fixture.receive_position], amount);
    fixture.receive_position += amount;
    return (int32_t)amount;
}

static int32_t fake_send(uint8_t socket_number, const uint8_t* data, uint16_t length)
{
    (void)socket_number;
    (void)data;
    fixture.send_calls++;
    TEST_ASSERT_LESS_OR_EQUAL_UINT16(TEST_SEND_CHUNK, length);
    fixture.sent_length += length;
    return (int32_t)length;
}

static management_http_send_status_t fake_send_status(uint8_t socket_number)
{
    (void)socket_number;
    return fixture.send_status;
}

static management_result_t fake_execute(void* context,
                                        const management_request_t* request,
                                        management_response_t* response)
{
    (void)context;
    fixture.execute_calls++;
    memset(response, 0, sizeof(*response));
    response->result = MANAGEMENT_RESULT_OK;
    response->value.kind = MANAGEMENT_VALUE_MODE;
    response->value.data.mode = OPERATING_MODE_GPIO;
    response->effect.kind = MANAGEMENT_EFFECT_SET;
    response->effect.resource = request->resource;
    return response->result;
}

static bool fake_apply(void* context, const management_effect_t* effect)
{
    (void)context;
    TEST_ASSERT_EQUAL_INT(MANAGEMENT_EFFECT_SET, effect->kind);
    fixture.apply_calls++;
    return true;
}

static const management_http_io_t fake_io = {
    .status = fake_status,
    .open = fake_open,
    .listen = fake_listen,
    .disconnect = fake_disconnect,
    .close = fake_close,
    .rx_available = fake_rx_available,
    .tx_available = fake_tx_available,
    .receive = fake_receive,
    .send = fake_send,
    .send_status = fake_send_status,
};

static void fixture_reset(void)
{
    memset(&fixture, 0, sizeof(fixture));
    fixture.send_status = MANAGEMENT_HTTP_SEND_COMPLETE;
}

void test_management_http_server_opens_socket_two_nonblocking_on_port_80(void)
{
    fixture_reset();
    fixture.status = SOCK_CLOSED;
    management_http_server_t server;
    management_http_server_init(&server, &fake_io, fake_execute, fake_apply, NULL);

    management_http_server_poll(&server);

    TEST_ASSERT_EQUAL_UINT(1U, fixture.open_calls);
    TEST_ASSERT_EQUAL_UINT8(MANAGEMENT_HTTP_SOCKET, fixture.open_socket);
    TEST_ASSERT_EQUAL_UINT8(Sn_MR_TCP, fixture.open_protocol);
    TEST_ASSERT_EQUAL_UINT16(MANAGEMENT_HTTP_PORT, fixture.open_port);
    TEST_ASSERT_BITS_HIGH(SF_IO_NONBLOCK, fixture.open_flags);
}

void test_management_http_server_listens_without_blocking(void)
{
    fixture_reset();
    fixture.status = SOCK_INIT;
    management_http_server_t server;
    management_http_server_init(&server, &fake_io, fake_execute, fake_apply, NULL);

    management_http_server_poll(&server);

    TEST_ASSERT_EQUAL_UINT(1U, fixture.listen_calls);
    TEST_ASSERT_EQUAL_UINT(0U, fixture.receive_calls);
    TEST_ASSERT_EQUAL_UINT(0U, fixture.send_calls);
}

void test_management_http_server_accumulates_fragments_and_does_one_io_per_poll(void)
{
    fixture_reset();
    static const uint8_t request[] = "GET /api/v1/system/mode HTTP/1.1\r\nHost: device\r\n\r\n";
    fixture.status = SOCK_ESTABLISHED;
    fixture.receive_data = request;
    fixture.receive_length = sizeof(request) - 1U;
    fixture.receive_limit = 9U;
    management_http_server_t server;
    management_http_server_init(&server, &fake_io, fake_execute, fake_apply, NULL);

    while (fixture.execute_calls == 0U)
    {
        unsigned int receive_calls_before = fixture.receive_calls;
        management_http_server_poll(&server);
        TEST_ASSERT_LESS_OR_EQUAL_UINT(receive_calls_before + 1U, fixture.receive_calls);
    }

    TEST_ASSERT_EQUAL_UINT(fixture.receive_length, fixture.receive_position);
    TEST_ASSERT_EQUAL_UINT(1U, fixture.execute_calls);
    TEST_ASSERT_EQUAL_UINT(0U, fixture.send_calls);
}

void test_management_http_server_sends_incrementally_then_applies_effect(void)
{
    fixture_reset();
    static const uint8_t request[] = "GET /api/v1/system/mode HTTP/1.1\r\n\r\n";
    fixture.status = SOCK_ESTABLISHED;
    fixture.receive_data = request;
    fixture.receive_length = sizeof(request) - 1U;
    management_http_server_t server;
    management_http_server_init(&server, &fake_io, fake_execute, fake_apply, NULL);

    management_http_server_poll(&server);
    TEST_ASSERT_EQUAL_UINT(1U, fixture.execute_calls);
    TEST_ASSERT_EQUAL_UINT(0U, fixture.apply_calls);

    for (unsigned int iteration = 0U;
         (server.response_sent < server.response_length) && (iteration < 64U);
         ++iteration)
    {
        unsigned int send_calls_before = fixture.send_calls;
        management_http_server_poll(&server);
        TEST_ASSERT_LESS_OR_EQUAL_UINT(send_calls_before + 1U, fixture.send_calls);
        TEST_ASSERT_EQUAL_UINT(0U, fixture.apply_calls);
    }

    TEST_ASSERT_EQUAL_UINT(0U, fixture.disconnect_calls);
    TEST_ASSERT_EQUAL_UINT(server.response_length, fixture.sent_length);
    TEST_ASSERT_EQUAL_UINT(0U, fixture.apply_calls);
    fixture.send_status = MANAGEMENT_HTTP_SEND_PENDING;
    management_http_server_poll(&server);
    TEST_ASSERT_EQUAL_UINT(0U, fixture.disconnect_calls);

    fixture.send_status = MANAGEMENT_HTTP_SEND_COMPLETE;
    management_http_server_poll(&server);
    TEST_ASSERT_EQUAL_UINT(1U, fixture.disconnect_calls);
    fixture.status = SOCK_FIN_WAIT;
    management_http_server_poll(&server);
    TEST_ASSERT_EQUAL_UINT(0U, fixture.apply_calls);
    TEST_ASSERT_TRUE(server.effect_pending);


    fixture.status = SOCK_CLOSED;
    management_http_server_poll(&server);
    TEST_ASSERT_EQUAL_UINT(1U, fixture.apply_calls);
}
