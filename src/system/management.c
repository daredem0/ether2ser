#include "system/management.h"

#include <string.h>

#include "system/management_catalog.h"

#define V24_INVERT_ALLOWED_MASK 0xFFU
#define LOG_LEVEL_ERROR_VALUE 1U
#define LOG_LEVEL_TRACE_VALUE 4U

static const management_resource_descriptor_t* descriptor_for_resource(
    management_resource_t resource)
{
    for (size_t i = 0U; i < management_catalog_count(); i++)
    {
        const management_resource_descriptor_t* descriptor = management_catalog_at(i);
        if (descriptor != NULL && descriptor->resource == resource)
        {
            return descriptor;
        }
    }
    return NULL;
}

static const gpio_signal_info_t* signal_info(gpio_signal_t signal)
{
    for (size_t i = 0U; i < gpio_control_signal_count(); i++)
    {
        const gpio_signal_info_t* info = gpio_control_signal_at(i);
        if (info != NULL && info->signal == signal)
        {
            return info;
        }
    }
    return NULL;
}

static bool baudrate_is_valid(uint32_t baudrate)
{
    switch (baudrate)
    {
    case 1200U:
    case 2400U:
    case 4800U:
    case 9600U:
    case 16000U:
    case 19200U:
    case 38400U:
    case 57600U:
    case 115200U:
        return true;
    default:
        return false;
    }
}

static management_result_t prepare_set_effect(const management_request_t* request,
                                              management_response_t* response,
                                              bool persist, bool reboot, bool accepted)
{
    response->changed         = true;
    response->value           = request->value;
    response->effect.kind     = MANAGEMENT_EFFECT_SET;
    response->effect.resource = request->resource;
    response->effect.value    = request->value;
    response->effect.persist  = persist;
    response->effect.reboot   = reboot;
    return accepted ? MANAGEMENT_RESULT_ACCEPTED : MANAGEMENT_RESULT_OK;
}

static management_result_t execute_mode(const management_request_t* request,
                                        const management_runtime_view_t* runtime,
                                        management_response_t* response)
{
    response->value.kind      = MANAGEMENT_VALUE_MODE;
    response->value.data.mode = runtime->operating_mode;
    if (request->method == MANAGEMENT_METHOD_GET)
    {
        return MANAGEMENT_RESULT_OK;
    }
    if (request->value.kind != MANAGEMENT_VALUE_MODE ||
        !operating_mode_is_valid(request->value.data.mode))
    {
        return MANAGEMENT_RESULT_INVALID_VALUE;
    }
    if (request->value.data.mode == runtime->operating_mode)
    {
        return MANAGEMENT_RESULT_OK;
    }
    if (!operating_mode_change_allowed(runtime->operating_mode, request->value.data.mode,
                                       runtime->frame_active, runtime->queue_empty))
    {
        return MANAGEMENT_RESULT_CONFLICT;
    }
    return prepare_set_effect(request, response, true, true, true);
}

static management_result_t execute_gpio(const management_request_t* request,
                                        const management_runtime_view_t* runtime,
                                        management_response_t* response)
{
    if (request->value.kind != MANAGEMENT_VALUE_GPIO)
    {
        return MANAGEMENT_RESULT_INVALID_VALUE;
    }
    const gpio_signal_info_t* info = signal_info(request->value.data.gpio.signal);
    if (info == NULL)
    {
        return MANAGEMENT_RESULT_INVALID_VALUE;
    }

    const uint32_t signal_mask = 1UL << (uint32_t)info->signal;
    if (request->method == MANAGEMENT_METHOD_GET)
    {
        response->value = request->value;
        response->value.data.gpio.level = (runtime->gpio_levels & signal_mask) != 0U;
        return MANAGEMENT_RESULT_OK;
    }
    if (runtime->operating_mode != OPERATING_MODE_GPIO)
    {
        return MANAGEMENT_RESULT_CONFLICT;
    }
    if (!gpio_control_manual_write_allowed(runtime->operating_mode, info->signal))
    {
        return MANAGEMENT_RESULT_READ_ONLY;
    }

    const bool old_level = (runtime->gpio_levels & signal_mask) != 0U;
    response->value      = request->value;
    if (old_level == request->value.data.gpio.level)
    {
        return MANAGEMENT_RESULT_OK;
    }
    return prepare_set_effect(request, response, false, false, false);
}

static management_result_t execute_network(const management_request_t* request,
                                           const management_runtime_view_t* runtime,
                                           management_response_t* response)
{
    bool changed = false;
    switch (request->resource)
    {
    case MANAGEMENT_RESOURCE_NET_IP_LOCAL:
        response->value.kind           = MANAGEMENT_VALUE_IPV4_CIDR;
        response->value.data.ipv4_cidr = runtime->local_ip;
        if (request->method == MANAGEMENT_METHOD_GET)
        {
            return MANAGEMENT_RESULT_OK;
        }
        if (request->value.kind != MANAGEMENT_VALUE_IPV4_CIDR ||
            request->value.data.ipv4_cidr.prefix_length > 32U)
        {
            return MANAGEMENT_RESULT_INVALID_VALUE;
        }
        changed = memcmp(&runtime->local_ip, &request->value.data.ipv4_cidr,
                         sizeof(runtime->local_ip)) != 0;
        break;
    case MANAGEMENT_RESOURCE_NET_IP_REMOTE:
    case MANAGEMENT_RESOURCE_NET_IP_GATEWAY:
    {
        const uint8_t* current = request->resource == MANAGEMENT_RESOURCE_NET_IP_REMOTE
                                     ? runtime->remote_ip
                                     : runtime->gateway;
        response->value.kind = MANAGEMENT_VALUE_IPV4;
        memcpy(response->value.data.ipv4, current, sizeof(response->value.data.ipv4));
        if (request->method == MANAGEMENT_METHOD_GET)
        {
            return MANAGEMENT_RESULT_OK;
        }
        if (request->value.kind != MANAGEMENT_VALUE_IPV4)
        {
            return MANAGEMENT_RESULT_INVALID_VALUE;
        }
        changed = memcmp(current, request->value.data.ipv4, sizeof(request->value.data.ipv4)) != 0;
        break;
    }
    case MANAGEMENT_RESOURCE_NET_UDP_PORT_LOCAL:
    case MANAGEMENT_RESOURCE_NET_UDP_PORT_REMOTE:
    {
        const uint16_t current =
            request->resource == MANAGEMENT_RESOURCE_NET_UDP_PORT_LOCAL
                ? runtime->local_udp_port
                : runtime->remote_udp_port;
        response->value.kind     = MANAGEMENT_VALUE_U16;
        response->value.data.u16 = current;
        if (request->method == MANAGEMENT_METHOD_GET)
        {
            return MANAGEMENT_RESULT_OK;
        }
        if (request->value.kind != MANAGEMENT_VALUE_U16 || request->value.data.u16 == 0U)
        {
            return MANAGEMENT_RESULT_INVALID_VALUE;
        }
        changed = current != request->value.data.u16;
        break;
    }
    default:
        return MANAGEMENT_RESULT_NOT_FOUND;
    }

    if (!changed)
    {
        return MANAGEMENT_RESULT_OK;
    }
    const bool disrupts_http = request->resource == MANAGEMENT_RESOURCE_NET_IP_LOCAL;
    return prepare_set_effect(request, response, true, false, disrupts_http);
}

static management_result_t execute_v24(const management_request_t* request,
                                       const management_runtime_view_t* runtime,
                                       management_response_t* response)
{
    bool changed = false;
    bool persist = true;
    bool reboot  = false;
    switch (request->resource)
    {
    case MANAGEMENT_RESOURCE_V24_BAUDRATE:
        response->value.kind     = MANAGEMENT_VALUE_U32;
        response->value.data.u32 = runtime->v24_baudrate;
        if (request->method == MANAGEMENT_METHOD_GET)
        {
            return MANAGEMENT_RESULT_OK;
        }
        if (request->value.kind != MANAGEMENT_VALUE_U32 ||
            !baudrate_is_valid(request->value.data.u32))
        {
            return MANAGEMENT_RESULT_INVALID_VALUE;
        }
        changed = runtime->v24_baudrate != request->value.data.u32;
        break;
    case MANAGEMENT_RESOURCE_V24_TXCLOCK:
        response->value.kind         = MANAGEMENT_VALUE_BOOL;
        response->value.data.boolean = runtime->v24_external_clock;
        if (request->method == MANAGEMENT_METHOD_GET)
        {
            return MANAGEMENT_RESULT_OK;
        }
        if (request->value.kind != MANAGEMENT_VALUE_BOOL)
        {
            return MANAGEMENT_RESULT_INVALID_VALUE;
        }
        changed = runtime->v24_external_clock != request->value.data.boolean;
        reboot  = changed;
        if (changed && (runtime->frame_active || !runtime->queue_empty))
        {
            return MANAGEMENT_RESULT_CONFLICT;
        }
        break;
    case MANAGEMENT_RESOURCE_V24_INVERT:
        response->value.kind     = MANAGEMENT_VALUE_SIGNAL_MASK;
        response->value.data.u32 = runtime->v24_invert_mask;
        if (request->method == MANAGEMENT_METHOD_GET)
        {
            return MANAGEMENT_RESULT_OK;
        }
        if (request->value.kind != MANAGEMENT_VALUE_SIGNAL_MASK ||
            (request->value.data.u32 & ~V24_INVERT_ALLOWED_MASK) != 0U)
        {
            return MANAGEMENT_RESULT_INVALID_VALUE;
        }
        changed = runtime->v24_invert_mask != request->value.data.u32;
        break;
    case MANAGEMENT_RESOURCE_V24_CTS_RESTART:
        response->value.kind         = MANAGEMENT_VALUE_BOOL;
        response->value.data.boolean = runtime->v24_cts_restart;
        if (request->method == MANAGEMENT_METHOD_GET)
        {
            return MANAGEMENT_RESULT_OK;
        }
        if (request->value.kind != MANAGEMENT_VALUE_BOOL)
        {
            return MANAGEMENT_RESULT_INVALID_VALUE;
        }
        changed = runtime->v24_cts_restart != request->value.data.boolean;
        persist = false;
        break;
    default:
        return MANAGEMENT_RESULT_NOT_FOUND;
    }

    if (!changed)
    {
        return MANAGEMENT_RESULT_OK;
    }
    return prepare_set_effect(request, response, persist, reboot, reboot);
}

static management_result_t execute_loglevel(const management_request_t* request,
                                            const management_runtime_view_t* runtime,
                                            management_response_t* response)
{
    response->value.kind     = MANAGEMENT_VALUE_U32;
    response->value.data.u32 = runtime->log_level;
    if (request->method == MANAGEMENT_METHOD_GET)
    {
        return MANAGEMENT_RESULT_OK;
    }
    if (request->value.kind != MANAGEMENT_VALUE_U32 ||
        request->value.data.u32 < LOG_LEVEL_ERROR_VALUE ||
        request->value.data.u32 > LOG_LEVEL_TRACE_VALUE)
    {
        return MANAGEMENT_RESULT_INVALID_VALUE;
    }
    if (request->value.data.u32 == runtime->log_level)
    {
        return MANAGEMENT_RESULT_OK;
    }
    return prepare_set_effect(request, response, false, false, false);
}

management_result_t management_execute(const management_request_t* request,
                                       const management_runtime_view_t* runtime,
                                       management_response_t* response)
{
    if (request == NULL || runtime == NULL || response == NULL)
    {
        return MANAGEMENT_RESULT_INTERNAL_ERROR;
    }
    memset(response, 0, sizeof(*response));
    response->result    = MANAGEMENT_RESULT_INTERNAL_ERROR;
    response->snapshot  = *runtime;

    const management_resource_descriptor_t* descriptor =
        descriptor_for_resource(request->resource);
    if (descriptor == NULL)
    {
        response->result = MANAGEMENT_RESULT_NOT_FOUND;
        return response->result;
    }
    const uint8_t method_mask = MANAGEMENT_METHOD_MASK(request->method);
    if ((descriptor->method_mask & method_mask) == 0U)
    {
        response->result = MANAGEMENT_RESULT_READ_ONLY;
        return response->result;
    }

    switch (request->resource)
    {
    case MANAGEMENT_RESOURCE_DISCOVERY:
    case MANAGEMENT_RESOURCE_STATUS:
    case MANAGEMENT_RESOURCE_MEMORY:
    case MANAGEMENT_RESOURCE_CONFIG:
    case MANAGEMENT_RESOURCE_NET:
    case MANAGEMENT_RESOURCE_V24:
    case MANAGEMENT_RESOURCE_PINS:
    case MANAGEMENT_RESOURCE_PIN:
        response->result = MANAGEMENT_RESULT_OK;
        break;
    case MANAGEMENT_RESOURCE_SYSTEM_MODE:
        response->result = execute_mode(request, runtime, response);
        break;
    case MANAGEMENT_RESOURCE_GPIO:
        response->result = execute_gpio(request, runtime, response);
        break;
    case MANAGEMENT_RESOURCE_NET_IP_LOCAL:
    case MANAGEMENT_RESOURCE_NET_IP_REMOTE:
    case MANAGEMENT_RESOURCE_NET_IP_GATEWAY:
    case MANAGEMENT_RESOURCE_NET_UDP_PORT_LOCAL:
    case MANAGEMENT_RESOURCE_NET_UDP_PORT_REMOTE:
        response->result = execute_network(request, runtime, response);
        break;
    case MANAGEMENT_RESOURCE_V24_BAUDRATE:
    case MANAGEMENT_RESOURCE_V24_TXCLOCK:
    case MANAGEMENT_RESOURCE_V24_INVERT:
    case MANAGEMENT_RESOURCE_V24_CTS_RESTART:
        response->result = execute_v24(request, runtime, response);
        break;
    case MANAGEMENT_RESOURCE_SYSTEM_LOGLEVEL:
        response->result = execute_loglevel(request, runtime, response);
        break;
    case MANAGEMENT_RESOURCE_ACTION_SAVE:
        response->effect.kind = MANAGEMENT_EFFECT_SAVE;
        response->result      = MANAGEMENT_RESULT_OK;
        break;
    case MANAGEMENT_RESOURCE_ACTION_WIPE:
        if (request->value.kind != MANAGEMENT_VALUE_TEXT || request->value.data.text == NULL ||
            strcmp(request->value.data.text, "wipe") != 0)
        {
            response->result = MANAGEMENT_RESULT_INVALID_VALUE;
            break;
        }
        response->effect.kind = MANAGEMENT_EFFECT_WIPE;
        response->result      = MANAGEMENT_RESULT_ACCEPTED;
        break;
    case MANAGEMENT_RESOURCE_ACTION_REBOOT:
        response->effect.kind   = MANAGEMENT_EFFECT_REBOOT;
        response->effect.reboot = true;
        response->result        = MANAGEMENT_RESULT_ACCEPTED;
        break;
    default:
        response->result = MANAGEMENT_RESULT_NOT_FOUND;
        break;
    }
    return response->result;
}

const char* management_result_name(management_result_t result)
{
    switch (result)
    {
    case MANAGEMENT_RESULT_OK:
        return "ok";
    case MANAGEMENT_RESULT_ACCEPTED:
        return "accepted";
    case MANAGEMENT_RESULT_NOT_FOUND:
        return "not_found";
    case MANAGEMENT_RESULT_READ_ONLY:
        return "read_only";
    case MANAGEMENT_RESULT_INVALID_VALUE:
        return "invalid_value";
    case MANAGEMENT_RESULT_CONFLICT:
        return "conflict";
    case MANAGEMENT_RESULT_BUSY:
        return "busy";
    case MANAGEMENT_RESULT_TOO_LARGE:
        return "too_large";
    case MANAGEMENT_RESULT_INTERNAL_ERROR:
    default:
        return "internal_error";
    }
}
