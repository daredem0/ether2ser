#ifndef MANAGEMENT_HTTP_SERVER_H
#define MANAGEMENT_HTTP_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "system/management_http.h"

#define MANAGEMENT_HTTP_SOCKET 2U
#define MANAGEMENT_HTTP_PORT 80U

typedef enum
{
    MANAGEMENT_HTTP_SEND_PENDING = 0,
    MANAGEMENT_HTTP_SEND_COMPLETE,
    MANAGEMENT_HTTP_SEND_FAILED,
} management_http_send_status_t;

typedef struct
{
    uint8_t (*status)(uint8_t socket_number);
    int8_t (*open)(uint8_t socket_number, uint8_t protocol, uint16_t port, uint8_t flags);
    int8_t (*listen)(uint8_t socket_number);
    int8_t (*disconnect)(uint8_t socket_number);
    int8_t (*close)(uint8_t socket_number);
    uint16_t (*rx_available)(uint8_t socket_number);
    uint16_t (*tx_available)(uint8_t socket_number);
    int32_t (*receive)(uint8_t socket_number, uint8_t* data, uint16_t length);
    int32_t (*send)(uint8_t socket_number, const uint8_t* data, uint16_t length);
    management_http_send_status_t (*send_status)(uint8_t socket_number);
} management_http_io_t;

typedef management_result_t (*management_http_execute_fn)(
    void* context, const management_request_t* request, management_response_t* response);
typedef bool (*management_http_apply_effect_fn)(void* context,
                                                const management_effect_t* effect);

typedef struct
{
    const management_http_io_t* io;
    management_http_execute_fn execute;
    management_http_apply_effect_fn apply_effect;
    void* context;
    uint8_t request[MANAGEMENT_HTTP_REQUEST_CAPACITY];
    char response[MANAGEMENT_HTTP_RESPONSE_CAPACITY];
    size_t request_length;
    size_t response_length;
    size_t response_sent;
    management_effect_t pending_effect;
    bool effect_pending;
    bool send_pending;
} management_http_server_t;

void management_http_server_init(management_http_server_t* server,
                                 const management_http_io_t* interface,
                                 management_http_execute_fn execute,
                                 management_http_apply_effect_fn apply_effect,
                                 void* context);
void management_http_server_poll(management_http_server_t* server);

#endif /* MANAGEMENT_HTTP_SERVER_H */
