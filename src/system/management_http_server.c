#include "system/management_http_server.h"

#include <limits.h>
#include <string.h>

#include "socket.h"
#include "w5500.h"

static void reset_transaction(management_http_server_t* server)
{
    server->request_length = 0U;
    server->response_length = 0U;
    server->response_sent = 0U;
    memset(&server->pending_effect, 0, sizeof(server->pending_effect));
    server->effect_pending = false;
    server->send_pending = false;
}

void management_http_server_init(management_http_server_t* server,
                                 const management_http_io_t* interface,
                                 management_http_execute_fn execute,
                                 management_http_apply_effect_fn apply_effect,
                                 void* context)
{
    if (server == NULL)
    {
        return;
    }
    memset(server, 0, sizeof(*server));
    server->io = interface;
    server->execute = execute;
    server->apply_effect = apply_effect;
    server->context = context;
}

static management_result_t parse_error_result(management_http_parse_result_t parse_result)
{
    switch (parse_result)
    {
        case MANAGEMENT_HTTP_PARSE_METHOD_NOT_ALLOWED:
            return MANAGEMENT_RESULT_READ_ONLY;
        case MANAGEMENT_HTTP_PARSE_NOT_FOUND:
            return MANAGEMENT_RESULT_NOT_FOUND;
        case MANAGEMENT_HTTP_PARSE_TOO_LARGE:
            return MANAGEMENT_RESULT_TOO_LARGE;
        case MANAGEMENT_HTTP_PARSE_BAD_REQUEST:
        case MANAGEMENT_HTTP_PARSE_INCOMPLETE:
        case MANAGEMENT_HTTP_PARSE_READY:
        default:
            return MANAGEMENT_RESULT_INVALID_VALUE;
    }
}

static void prepare_error_response(management_http_server_t* server,
                                   management_result_t result)
{
    management_response_t response;
    memset(&response, 0, sizeof(response));
    response.result = result;
    if (!management_http_format_response("request", &response, server->response,
                                         sizeof(server->response),
                                         &server->response_length))
    {
        server->response_length = 0U;
    }
    server->response_sent = 0U;
}

static void prepare_management_response(management_http_server_t* server,
                                        const management_request_t* request,
                                        const char* canonical_path)
{
    management_response_t response;
    memset(&response, 0, sizeof(response));
    response.result = server->execute(server->context, request, &response);
    if (!management_http_format_response(canonical_path, &response, server->response,
                                         sizeof(server->response),
                                         &server->response_length))
    {
        prepare_error_response(server, MANAGEMENT_RESULT_INTERNAL_ERROR);
        return;
    }
    server->response_sent = 0U;
    if (response.effect.kind != MANAGEMENT_EFFECT_NONE)
    {
        server->pending_effect = response.effect;
        server->effect_pending = true;
    }
}

static void receive_request(management_http_server_t* server)
{
    const uint16_t available = server->io->rx_available(MANAGEMENT_HTTP_SOCKET);
    if (available == 0U)
    {
        return;
    }
    const size_t remaining = sizeof(server->request) - server->request_length;
    if (remaining == 0U)
    {
        prepare_error_response(server, MANAGEMENT_RESULT_TOO_LARGE);
        return;
    }
    uint16_t amount = available;
    if ((size_t)amount > remaining)
    {
        amount = (uint16_t)remaining;
    }
    const int32_t received = server->io->receive(
        MANAGEMENT_HTTP_SOCKET, &server->request[server->request_length], amount);
    if (received <= 0)
    {
        return;
    }
    if ((uint32_t)received > amount)
    {
        prepare_error_response(server, MANAGEMENT_RESULT_INTERNAL_ERROR);
        return;
    }
    server->request_length += (size_t)received;

    management_request_t request;
    const char* canonical_path = NULL;
    const management_http_parse_result_t parse_result = management_http_parse_request(
        server->request, server->request_length, &request, &canonical_path);
    if (parse_result == MANAGEMENT_HTTP_PARSE_INCOMPLETE)
    {
        if (server->request_length == sizeof(server->request))
        {
            prepare_error_response(server, MANAGEMENT_RESULT_TOO_LARGE);
        }
        return;
    }
    if (parse_result != MANAGEMENT_HTTP_PARSE_READY)
    {
        prepare_error_response(server, parse_error_result(parse_result));
        return;
    }
    prepare_management_response(server, &request, canonical_path);
}

static void send_response(management_http_server_t* server)
{
    if (server->send_pending)
    {
        const management_http_send_status_t send_status =
            server->io->send_status(MANAGEMENT_HTTP_SOCKET);
        if (send_status == MANAGEMENT_HTTP_SEND_PENDING)
        {
            return;
        }
        server->send_pending = false;
        if (send_status == MANAGEMENT_HTTP_SEND_FAILED)
        {
            server->effect_pending = false;
            (void)server->io->close(MANAGEMENT_HTTP_SOCKET);
            return;
        }
    }
    if (server->response_sent >= server->response_length)
    {
        (void)server->io->disconnect(MANAGEMENT_HTTP_SOCKET);
        return;
    }
    const uint16_t available = server->io->tx_available(MANAGEMENT_HTTP_SOCKET);
    if (available == 0U)
    {
        return;
    }
    const size_t remaining = server->response_length - server->response_sent;
    uint16_t amount = available;
    if ((size_t)amount > remaining)
    {
        amount = (uint16_t)remaining;
    }
    const int32_t sent = server->io->send(
        MANAGEMENT_HTTP_SOCKET,
        (const uint8_t*)&server->response[server->response_sent], amount);
    if ((sent <= 0) || ((uint32_t)sent > amount))
    {
        return;
    }
    server->response_sent += (size_t)sent;
    server->send_pending = true;
}

void management_http_server_poll(management_http_server_t* server)
{
    if ((server == NULL) || (server->io == NULL) || (server->execute == NULL) ||
        (server->apply_effect == NULL))
    {
        return;
    }
    const uint8_t status = server->io->status(MANAGEMENT_HTTP_SOCKET);
    switch (status)
    {
        case SOCK_CLOSED:
            if (server->effect_pending)
            {
                (void)server->apply_effect(server->context, &server->pending_effect);
                server->effect_pending = false;
            }
            reset_transaction(server);
            (void)server->io->open(MANAGEMENT_HTTP_SOCKET, Sn_MR_TCP,
                                   MANAGEMENT_HTTP_PORT, SF_IO_NONBLOCK);
            break;
        case SOCK_INIT:
            (void)server->io->listen(MANAGEMENT_HTTP_SOCKET);
            break;
        case SOCK_LISTEN:
            break;
        case SOCK_ESTABLISHED:
            if (server->response_length > 0U)
            {
                send_response(server);
            }
            else
            {
                receive_request(server);
            }
            break;
        case SOCK_SYNSENT:
        case SOCK_SYNRECV:
        case SOCK_FIN_WAIT:
        case SOCK_TIME_WAIT:
        case SOCK_LAST_ACK:
            break;
        case SOCK_CLOSE_WAIT:
            (void)server->io->disconnect(MANAGEMENT_HTTP_SOCKET);
            break;
        default:
            (void)server->io->close(MANAGEMENT_HTTP_SOCKET);
            break;
    }
}
