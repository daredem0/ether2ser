#include "system/management_http.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "drivers/gpio_control.h"
#include "system/management_cli.h"
#include "system/management_catalog.h"
#include "system/operating_mode.h"

#define HTTP_REQUEST_LINE_CAPACITY 192U
#define HTTP_ROUTE_CAPACITY 96U
#define HTTP_VALUE_CAPACITY 64U

static bool ascii_equal_ci(const char* left, size_t left_length, const char* right)
{
    const size_t right_length = strlen(right);
    if (left_length != right_length)
    {
        return false;
    }
    for (size_t i = 0U; i < left_length; ++i)
    {
        char a = left[i];
        char b = right[i];
        if ((a >= 'A') && (a <= 'Z'))
        {
            a = (char)(a + ('a' - 'A'));
        }
        if ((b >= 'A') && (b <= 'Z'))
        {
            b = (char)(b + ('a' - 'A'));
        }
        if (a != b)
        {
            return false;
        }
    }
    return true;
}

static const uint8_t* find_bytes(const uint8_t* data, size_t data_length,
                                 const char* needle, size_t needle_length)
{
    if ((data == NULL) || (needle == NULL) || (needle_length == 0U) ||
        (data_length < needle_length))
    {
        return NULL;
    }
    for (size_t i = 0U; i <= (data_length - needle_length); ++i)
    {
        if (memcmp(&data[i], needle, needle_length) == 0)
        {
            return &data[i];
        }
    }
    return NULL;
}

static bool parse_decimal_size(const uint8_t* text, size_t length, size_t* value)
{
    if ((text == NULL) || (value == NULL) || (length == 0U))
    {
        return false;
    }
    size_t parsed = 0U;
    for (size_t i = 0U; i < length; ++i)
    {
        if ((text[i] < (uint8_t)'0') || (text[i] > (uint8_t)'9'))
        {
            return false;
        }
        const size_t digit = (size_t)(text[i] - (uint8_t)'0');
        if (parsed > ((MANAGEMENT_HTTP_REQUEST_CAPACITY - digit) / 10U))
        {
            return false;
        }
        parsed = (parsed * 10U) + digit;
    }
    *value = parsed;
    return true;
}

static void trim_ascii_space(const uint8_t** text, size_t* length)
{
    while ((*length > 0U) && (((**text) == (uint8_t)' ') ||
                              ((**text) == (uint8_t)'\t')))
    {
        ++(*text);
        --(*length);
    }
    while ((*length > 0U) && (((*text)[*length - 1U] == (uint8_t)' ') ||
                              ((*text)[*length - 1U] == (uint8_t)'\t')))
    {
        --(*length);
    }
}

static bool parse_headers(const uint8_t* first_header, const uint8_t* header_end,
                          size_t* content_length)
{
    *content_length = 0U;
    bool content_length_seen = false;
    if (first_header > header_end)
    {
        return true;
    }
    const uint8_t* cursor = first_header;
    while (cursor < header_end)
    {
        const size_t remaining = (size_t)(header_end - cursor);
        const uint8_t* line_end = find_bytes(cursor, remaining, "\r\n", 2U);
        if (line_end == NULL)
        {
            line_end = header_end;
        }
        const uint8_t* colon = memchr(cursor, ':', (size_t)(line_end - cursor));
        if (colon == NULL)
        {
            return false;
        }
        const uint8_t* name = cursor;
        size_t name_length = (size_t)(colon - cursor);
        trim_ascii_space(&name, &name_length);
        const uint8_t* value = colon + 1;
        size_t value_length = (size_t)(line_end - value);
        trim_ascii_space(&value, &value_length);

        if (ascii_equal_ci((const char*)name, name_length, "Content-Length"))
        {
            if (content_length_seen ||
                !parse_decimal_size(value, value_length, content_length))
            {
                return false;
            }
            content_length_seen = true;
        }
        else if (ascii_equal_ci((const char*)name, name_length, "Transfer-Encoding"))
        {
            return false;
        }
        cursor = (line_end == header_end) ? header_end : (line_end + 2U);
    }
    return true;
}

static bool uri_to_route(const char* uri, char* route, size_t route_capacity)
{
    static const char prefix[] = "/api/v1";
    const size_t prefix_length = sizeof(prefix) - 1U;
    if (strncmp(uri, prefix, prefix_length) != 0)
    {
        return false;
    }
    const char* suffix = uri + prefix_length;
    if (*suffix == '\0')
    {
        route[0] = '\0';
        return true;
    }
    if (*suffix != '/')
    {
        return false;
    }
    ++suffix;
    const size_t suffix_length = strlen(suffix);
    if ((suffix_length == 0U) || (suffix_length >= route_capacity))
    {
        return false;
    }
    for (size_t i = 0U; i < suffix_length; ++i)
    {
        const char character = suffix[i];
        if (character == '/')
        {
            if ((i == 0U) || (i == (suffix_length - 1U)) || (suffix[i - 1U] == '/'))
            {
                return false;
            }
            route[i] = '.';
        }
        else if (((character >= 'a') && (character <= 'z')) ||
                 ((character >= '0') && (character <= '9')) ||
                 (character == '_') || (character == '-'))
        {
            route[i] = character;
        }
        else
        {
            return false;
        }
    }
    route[suffix_length] = '\0';
    return true;
}

static bool extract_json_string_or_scalar(const uint8_t* body, size_t body_length,
                                          const char* member, char* value,
                                          size_t value_capacity)
{
    size_t position = 0U;
#define SKIP_JSON_SPACE()                                                                \
    do                                                                                   \
    {                                                                                    \
        while ((position < body_length) &&                                               \
               ((body[position] == (uint8_t)' ') || (body[position] == (uint8_t)'\t') || \
                (body[position] == (uint8_t)'\r') || (body[position] == (uint8_t)'\n'))) \
        {                                                                                \
            ++position;                                                                  \
        }                                                                                \
    } while (false)

    SKIP_JSON_SPACE();
    if ((position >= body_length) || (body[position] != (uint8_t)'{'))
    {
        return false;
    }
    ++position;
    SKIP_JSON_SPACE();
    const size_t member_length = strlen(member);
    if ((position >= body_length) || (body[position] != (uint8_t)'\"'))
    {
        return false;
    }
    ++position;
    if (((body_length - position) < member_length) ||
        (memcmp(&body[position], member, member_length) != 0))
    {
        return false;
    }
    position += member_length;
    if ((position >= body_length) || (body[position] != (uint8_t)'\"'))
    {
        return false;
    }
    ++position;
    SKIP_JSON_SPACE();
    if ((position >= body_length) || (body[position] != (uint8_t)':'))
    {
        return false;
    }
    ++position;
    SKIP_JSON_SPACE();

    size_t value_length = 0U;
    if ((position < body_length) && (body[position] == (uint8_t)'\"'))
    {
        ++position;
        const size_t start = position;
        while ((position < body_length) && (body[position] != (uint8_t)'\"'))
        {
            if ((body[position] == (uint8_t)'\\') || (body[position] < 0x20U))
            {
                return false;
            }
            ++position;
        }
        if (position >= body_length)
        {
            return false;
        }
        value_length = position - start;
        if ((value_length == 0U) || (value_length >= value_capacity))
        {
            return false;
        }
        memcpy(value, &body[start], value_length);
        ++position;
    }
    else
    {
        const size_t start = position;
        while ((position < body_length) &&
               (body[position] != (uint8_t)'}') &&
               (body[position] != (uint8_t)' ') &&
               (body[position] != (uint8_t)'\t') &&
               (body[position] != (uint8_t)'\r') &&
               (body[position] != (uint8_t)'\n'))
        {
            ++position;
        }
        value_length = position - start;
        if ((value_length == 0U) || (value_length >= value_capacity))
        {
            return false;
        }
        memcpy(value, &body[start], value_length);
    }
    value[value_length] = '\0';
    if (strcmp(value, "true") == 0)
    {
        value[0] = '1';
        value[1] = '\0';
    }
    else if (strcmp(value, "false") == 0)
    {
        value[0] = '0';
        value[1] = '\0';
    }

    SKIP_JSON_SPACE();
    if ((position >= body_length) || (body[position] != (uint8_t)'}'))
    {
        return false;
    }
    ++position;
    SKIP_JSON_SPACE();
#undef SKIP_JSON_SPACE
    return position == body_length;
}

static management_http_parse_result_t map_cli_result(management_result_t result)
{
    if (result == MANAGEMENT_RESULT_OK)
    {
        return MANAGEMENT_HTTP_PARSE_READY;
    }
    if (result == MANAGEMENT_RESULT_NOT_FOUND)
    {
        return MANAGEMENT_HTTP_PARSE_NOT_FOUND;
    }
    if (result == MANAGEMENT_RESULT_TOO_LARGE)
    {
        return MANAGEMENT_HTTP_PARSE_TOO_LARGE;
    }
    return MANAGEMENT_HTTP_PARSE_BAD_REQUEST;
}

static management_http_parse_result_t parse_route_request(
    const char* method, const char* route, const uint8_t* body, size_t body_length,
    management_request_t* request, const char** canonical_path)
{
    char command[MANAGEMENT_CLI_MAX_LINE + 1U];
    char value[HTTP_VALUE_CAPACITY];
    int written = -1;

    if (strcmp(method, "GET") == 0)
    {
        if (body_length != 0U)
        {
            return MANAGEMENT_HTTP_PARSE_BAD_REQUEST;
        }
        written = (route[0] == '\0') ? snprintf(command, sizeof(command), "help")
                                     : snprintf(command, sizeof(command), "get %s", route);
    }
    else if (strcmp(method, "PUT") == 0)
    {
        if ((route[0] == '\0') ||
            !extract_json_string_or_scalar(body, body_length, "value", value,
                                           sizeof(value)))
        {
            return MANAGEMENT_HTTP_PARSE_BAD_REQUEST;
        }
        written = snprintf(command, sizeof(command), "set %s %s", route, value);
    }
    else if (strcmp(method, "POST") == 0)
    {
        if (strcmp(route, "actions.save") == 0)
        {
            if (body_length != 0U)
            {
                return MANAGEMENT_HTTP_PARSE_BAD_REQUEST;
            }
            written = snprintf(command, sizeof(command), "save");
        }
        else if (strcmp(route, "actions.reset-stats") == 0)
        {
            if (body_length != 0U)
            {
                return MANAGEMENT_HTTP_PARSE_BAD_REQUEST;
            }
            written = snprintf(command, sizeof(command), "reset stats");
        }
        else if (strcmp(route, "actions.reboot") == 0)
        {
            if (body_length != 0U)
            {
                return MANAGEMENT_HTTP_PARSE_BAD_REQUEST;
            }
            written = snprintf(command, sizeof(command), "reboot");
        }
        else if (strcmp(route, "actions.wipe") == 0)
        {
            if (!extract_json_string_or_scalar(body, body_length, "confirm", value,
                                               sizeof(value)))
            {
                return MANAGEMENT_HTTP_PARSE_BAD_REQUEST;
            }
            written = snprintf(command, sizeof(command), "wipe %s", value);
        }
        else
        {
            return MANAGEMENT_HTTP_PARSE_NOT_FOUND;
        }
    }
    else
    {
        return MANAGEMENT_HTTP_PARSE_METHOD_NOT_ALLOWED;
    }

    if ((written < 0) || ((size_t)written >= sizeof(command)))
    {
        return MANAGEMENT_HTTP_PARSE_TOO_LARGE;
    }
    return map_cli_result(management_cli_parse(command, request, canonical_path));
}

management_http_parse_result_t management_http_parse_request(
    const uint8_t* data, size_t data_length, management_request_t* request,
    const char** canonical_path)
{
    if ((data == NULL) || (request == NULL) || (canonical_path == NULL))
    {
        return MANAGEMENT_HTTP_PARSE_BAD_REQUEST;
    }
    if (data_length > MANAGEMENT_HTTP_REQUEST_CAPACITY)
    {
        return MANAGEMENT_HTTP_PARSE_TOO_LARGE;
    }

    const uint8_t* headers_end = find_bytes(data, data_length, "\r\n\r\n", 4U);
    if (headers_end == NULL)
    {
        return MANAGEMENT_HTTP_PARSE_INCOMPLETE;
    }
    const uint8_t* request_line_end = find_bytes(
        data, (size_t)(headers_end - data) + 2U, "\r\n", 2U);
    if (request_line_end == NULL)
    {
        return MANAGEMENT_HTTP_PARSE_BAD_REQUEST;
    }

    const uint8_t* first_space = memchr(data, ' ', (size_t)(request_line_end - data));
    if (first_space == NULL)
    {
        return MANAGEMENT_HTTP_PARSE_BAD_REQUEST;
    }
    const uint8_t* uri_start = first_space + 1U;
    const uint8_t* second_space = memchr(uri_start, ' ',
                                        (size_t)(request_line_end - uri_start));
    if ((second_space == NULL) ||
        ((size_t)(request_line_end - second_space - 1U) != 8U) ||
        (memcmp(second_space + 1U, "HTTP/1.1", 8U) != 0))
    {
        return MANAGEMENT_HTTP_PARSE_BAD_REQUEST;
    }

    const size_t method_length = (size_t)(first_space - data);
    char method[8];
    if ((method_length == 0U) || (method_length >= sizeof(method)))
    {
        return MANAGEMENT_HTTP_PARSE_METHOD_NOT_ALLOWED;
    }
    memcpy(method, data, method_length);
    method[method_length] = '\0';

    const size_t uri_length = (size_t)(second_space - uri_start);
    char uri[HTTP_ROUTE_CAPACITY + 8U];
    if ((uri_length == 0U) || (uri_length >= sizeof(uri)))
    {
        return MANAGEMENT_HTTP_PARSE_BAD_REQUEST;
    }
    memcpy(uri, uri_start, uri_length);
    uri[uri_length] = '\0';

    size_t content_length = 0U;
    if (!parse_headers(request_line_end + 2U, headers_end, &content_length))
    {
        return MANAGEMENT_HTTP_PARSE_BAD_REQUEST;
    }
    const uint8_t* body = headers_end + 4U;
    const size_t header_length = (size_t)(body - data);
    if (content_length > (MANAGEMENT_HTTP_REQUEST_CAPACITY - header_length))
    {
        return MANAGEMENT_HTTP_PARSE_TOO_LARGE;
    }
    if (data_length < (header_length + content_length))
    {
        return MANAGEMENT_HTTP_PARSE_INCOMPLETE;
    }
    if (data_length != (header_length + content_length))
    {
        return MANAGEMENT_HTTP_PARSE_BAD_REQUEST;
    }

    char route[HTTP_ROUTE_CAPACITY];
    if (!uri_to_route(uri, route, sizeof(route)))
    {
        return MANAGEMENT_HTTP_PARSE_BAD_REQUEST;
    }
    return parse_route_request(method, route, body, content_length, request,
                               canonical_path);
}

static bool append_format(char* output, size_t output_capacity, size_t* used,
                          const char* format, ...)
{
    if (*used >= output_capacity)
    {
        return false;
    }
    va_list arguments;
    va_start(arguments, format);
    const int written = vsnprintf(&output[*used], output_capacity - *used, format, arguments);
    va_end(arguments);
    if ((written < 0) || ((size_t)written >= (output_capacity - *used)))
    {
        return false;
    }
    *used += (size_t)written;
    return true;
}

static const char* http_status(management_result_t result)
{
    switch (result)
    {
        case MANAGEMENT_RESULT_OK: return "200 OK";
        case MANAGEMENT_RESULT_ACCEPTED: return "202 Accepted";
        case MANAGEMENT_RESULT_NOT_FOUND: return "404 Not Found";
        case MANAGEMENT_RESULT_READ_ONLY: return "405 Method Not Allowed";
        case MANAGEMENT_RESULT_INVALID_VALUE: return "400 Bad Request";
        case MANAGEMENT_RESULT_CONFLICT: return "409 Conflict";
        case MANAGEMENT_RESULT_BUSY: return "503 Service Unavailable";
        case MANAGEMENT_RESULT_TOO_LARGE: return "413 Payload Too Large";
        case MANAGEMENT_RESULT_INTERNAL_ERROR:
        default: return "500 Internal Server Error";
    }
}

static const char* gpio_signal_name(gpio_signal_t signal)
{
    for (size_t index = 0U; index < gpio_control_signal_count(); ++index)
    {
        const gpio_signal_info_t* info = gpio_control_signal_at(index);
        if ((info != NULL) && (info->signal == signal))
        {
            return info->name;
        }
    }
    return "unknown";
}
static bool append_network_value(char* body, size_t capacity, size_t* used,
                                 const management_runtime_view_t* view)
{
    return append_format(
        body, capacity, used,
        ",\"value\":{\"local\":\"%u.%u.%u.%u/%u\","
        "\"remote\":\"%u.%u.%u.%u\",\"gateway\":\"%u.%u.%u.%u\","
        "\"udp_port_local\":%u,\"udp_port_remote\":%u}",
        view->local_ip.address[0], view->local_ip.address[1],
        view->local_ip.address[2], view->local_ip.address[3],
        view->local_ip.prefix_length, view->remote_ip[0], view->remote_ip[1],
        view->remote_ip[2], view->remote_ip[3], view->gateway[0], view->gateway[1],
        view->gateway[2], view->gateway[3], (unsigned int)view->local_udp_port,
        (unsigned int)view->remote_udp_port);
}

static bool append_v24_value(char* body, size_t capacity, size_t* used,
                             const management_runtime_view_t* view)
{
    return append_format(
        body, capacity, used,
        ",\"value\":{\"baudrate\":%" PRIu32 ",\"txclock\":\"%s\","
        "\"invert\":%" PRIu32 ",\"ctsrestart\":%s}",
        view->v24_baudrate, view->v24_external_clock ? "external" : "internal",
        view->v24_invert_mask, view->v24_cts_restart ? "true" : "false");
}

static bool append_memory_value(char* body, size_t capacity, size_t* used,
                               const management_memory_snapshot_t* memory)
{
    return append_format(
        body, capacity, used,
        ",\"value\":{\"ram_static_used\":%" PRIu32 ","
        "\"heap_allocated\":%" PRIu32 ",\"ram_total\":%" PRIu32 ","
        "\"ram_free\":%" PRIu32 ",\"flash_used\":%" PRIu32 ","
        "\"flash_total\":%" PRIu32 ",\"flash_free\":%" PRIu32 "}",
        memory->ram_static_used, memory->heap_allocated, memory->ram_total,
        memory->ram_free, memory->flash_used, memory->flash_total,
        memory->flash_free);
}

static bool append_config_value(char* body, size_t capacity, size_t* used,
                               const management_runtime_view_t* view)
{
    return append_format(
        body, capacity, used,
        ",\"value\":{\"mode\":\"%s\",\"loglevel\":%" PRIu32 ","
        "\"net\":{\"local\":\"%u.%u.%u.%u/%u\","
        "\"remote\":\"%u.%u.%u.%u\",\"gateway\":\"%u.%u.%u.%u\","
        "\"udp_port_local\":%u,\"udp_port_remote\":%u},"
        "\"v24\":{\"baudrate\":%" PRIu32 ",\"txclock\":\"%s\","
        "\"invert\":%" PRIu32 ",\"ctsrestart\":%s}}",
        operating_mode_name(view->operating_mode), view->log_level,
        view->local_ip.address[0], view->local_ip.address[1],
        view->local_ip.address[2], view->local_ip.address[3],
        view->local_ip.prefix_length, view->remote_ip[0], view->remote_ip[1],
        view->remote_ip[2], view->remote_ip[3], view->gateway[0], view->gateway[1],
        view->gateway[2], view->gateway[3], (unsigned int)view->local_udp_port,
        (unsigned int)view->remote_udp_port, view->v24_baudrate,
        view->v24_external_clock ? "external" : "internal", view->v24_invert_mask,
        view->v24_cts_restart ? "true" : "false");
}

static bool append_pins_value(char* body, size_t capacity, size_t* used,
                             const management_runtime_view_t* view)
{
    if (!append_format(body, capacity, used, ",\"value\":["))
    {
        return false;
    }
    for (size_t index = 0U; index < gpio_control_signal_count(); ++index)
    {
        const gpio_signal_info_t* signal = gpio_control_signal_at(index);
        if (signal == NULL)
        {
            continue;
        }
        const bool level =
            (view->gpio_levels & (1UL << (uint32_t)signal->signal)) != 0U;
        if (!append_format(
                body, capacity, used,
                "%s{\"name\":\"%s\",\"gpio\":%u,\"direction\":\"%s\","
                "\"reserved\":%s,\"level\":%s}",
                index == 0U ? "" : ",", signal->name, signal->gpio_num,
                signal->direction == GPIO_SIGNAL_OUTPUT ? "output" : "input",
                signal->reserved ? "true" : "false", level ? "true" : "false"))
        {
            return false;
        }
    }
    return append_format(body, capacity, used, "]");
}

static bool append_discovery_value(char* body, size_t capacity, size_t* used)
{
    if (!append_format(body, capacity, used, ",\"value\":["))
    {
        return false;
    }
    for (size_t index = 0U; index < management_catalog_count(); ++index)
    {
        const management_resource_descriptor_t* descriptor =
            management_catalog_at(index);
        if ((descriptor == NULL) ||
            !append_format(body, capacity, used,
                           "%s{\"path\":\"%s\",\"methods\":%u,\"type\":%u}",
                           index == 0U ? "" : ",", descriptor->path,
                           descriptor->method_mask,
                           (unsigned int)descriptor->value_kind))
        {
            return false;
        }
    }
    return append_format(body, capacity, used, "]");
}

static bool append_status_value(char* body, size_t capacity, size_t* used,
                               const management_runtime_view_t* view)
{
    const management_status_snapshot_t* status = &view->status;
    const payload_statistics_t* stats = &status->stats;
    return append_format(
        body, capacity, used,
        ",\"value\":{\"mode\":\"%s\","
        "\"clock\":{\"rck_hz\":%.1f,\"xck_hz\":%.1f,\"xck_available\":%s,"
        "\"dma_baudrate_hz\":%" PRIu64 "},"
        "\"frames\":{\"udp_rx_frames\":%" PRIu64 ",\"hdlc_tx_frames\":%" PRIu64
        ",\"hdlc_rx_frames\":%" PRIu64 "},"
        "\"udp_tx\":{\"attempts\":%" PRIu64 ",\"ok\":%" PRIu64
        ",\"fail\":%" PRIu64 "},"
        "\"backlog\":{\"tx_ready_gap\":%" PRIu64 ",\"ready_udp_gap\":%" PRIu64 "},"
        "\"serial\":{\"rx_bytes\":%" PRIu64 ",\"tx_bytes\":%" PRIu64 "},"
        "\"rates\":{\"udp_rx\":%" PRIu64 ",\"hdlc_rx\":%" PRIu64
        ",\"udp_tx\":%" PRIu64 ",\"decode_fail\":%" PRIu64
        ",\"serial_rx_bytes\":%" PRIu64 "},"
        "\"decode\":{\"ready\":%" PRIu64 ",\"ok\":%" PRIu64
        ",\"fail\":%" PRIu64 ",\"invalid\":%" PRIu64
        ",\"short\":%" PRIu64 ",\"long\":%" PRIu64
        ",\"unstuff\":%" PRIu64 ",\"crc\":%" PRIu64 ","
        "\"first_failure\":{\"valid\":%s,\"reason\":%u,\"encoded_length\":%zu,"
        "\"raw_bit_index\":%zu,\"decoded_length\":%zu,\"received_crc\":%u,"
        "\"calculated_crc\":%u,\"sync_bit_offset\":%u,"
        "\"align_shift_right\":%s}},"
        "\"sync\":{\"state\":%d,\"wait_syncing\":%" PRIu64
        ",\"wait_synced\":%" PRIu64 ",\"consume\":%" PRIu64
        ",\"hardcap_events\":%" PRIu64 ",\"hardcap_bytes\":%" PRIu64 "},"
        "\"resync\":{\"idle\":%" PRIu64 ",\"hard\":%" PRIu64
        ",\"no_progress\":%" PRIu64 "},"
        "\"accumulator\":{\"position\":%zu,\"processed\":%zu,\"bit_offset\":%u,"
        "\"candidate_valid\":%s,\"candidate_end\":%zu},"
        "\"rx\":{\"accumulator_max\":%" PRIu64 ",\"fifo_stall\":%" PRIu64
        ",\"drop_acc_full\":%" PRIu64 ",\"hunt_idle_drop\":%" PRIu64
        ",\"dma_overflow_words\":%" PRIu32 "},"
        "\"queue\":{\"used\":%zu,\"capacity\":%zu,\"active\":%s},"
        "\"highwater\":{\"tx\":%" PRIu64 ",\"event\":%" PRIu64
        ",\"log\":%" PRIu64 "},"
        "\"w5500\":{\"rx_no_room\":%" PRIu64 ",\"tx_full\":%" PRIu64 "},"
        "\"drops\":{\"tx\":%" PRIu64 ",\"event\":%" PRIu64
        ",\"log\":%" PRIu64 "},"
        "\"reconstructed_length\":%zu,"
        "\"throttle\":{\"enter\":%" PRIu64 ",\"skips\":%" PRIu64 "},"
        "\"pio\":{\"stalled\":%s,\"tx_fifo_underruns\":%" PRIu32
        ",\"cts_restarts\":%" PRIu32 "},"
        "\"cts\":{\"level\":%s,\"edges\":%" PRIu32 ",\"restart\":%s}}",
        operating_mode_name(view->operating_mode), status->rck_hz, status->xck_hz,
        status->xck_available ? "true" : "false", status->rates.dma_baudrate_hz,
        stats->udp_rx_frames, stats->hdlc_tx_frames, stats->hdlc_frame_ready,
        stats->udp_tx_attempt_frames, stats->udp_tx_success_frames,
        stats->udp_tx_failed_frames, status->tx_ready_gap, status->ready_udp_gap,
        stats->serial_rx_bytes, status->tx_wire_bytes,
        status->rates.udp_rx_rate_fps, status->rates.hdlc_rx_rate_fps,
        status->rates.udp_tx_rate_fps, status->rates.decode_fail_rate,
        status->rates.serial_rx_rate_bps, stats->hdlc_frame_ready,
        stats->hdlc_decode_ok, stats->hdlc_decode_fail,
        stats->decode_fail_invalid_frame, stats->decode_fail_too_short,
        stats->decode_fail_payload_too_long, stats->decode_fail_unstuff_error,
        stats->decode_fail_crc_mismatch,
        status->first_decode_failure.valid ? "true" : "false",
        (unsigned int)status->first_decode_failure.reason,
        status->first_decode_failure.encoded_length,
        status->first_decode_failure.raw_bit_index,
        status->first_decode_failure.decoded_length,
        (unsigned int)status->first_decode_failure.received_crc,
        (unsigned int)status->first_decode_failure.calculated_crc,
        status->first_decode_failure.sync_bit_offset,
        status->first_decode_failure.align_shift_right ? "true" : "false",
        status->sync_state,
        stats->sync_lookahead_wait_syncing, stats->sync_lookahead_wait_synced,
        stats->sync_candidate_consume, stats->sync_hardcap_drop_events,
        stats->sync_hardcap_drop_bytes, stats->resync_idle_timeout_count,
        stats->resync_hard_fail_count, stats->resync_no_progress_count,
        status->accumulator_position, status->accumulator_processed,
        status->accumulator_bit_offset,
        status->accumulator_candidate_valid ? "true" : "false",
        status->accumulator_candidate_end, stats->accumulator_pos_max,
        stats->rx_fifo_stall_events, stats->serial_rx_drop_acc_full,
        stats->hunt_idle_drop_bytes, status->dma_overflow_words,
        status->tx_queue_used, status->tx_queue_capacity,
        status->tx_frame_active ? "true" : "false", stats->tx_queue_used_max,
        stats->event_queue_used_max, stats->log_queue_used_max,
        stats->udp_rx_buffer_full_counts, stats->udp_tx_buffer_full_counts,
        stats->tx_queue_drop_frames, stats->event_queue_drop_events,
        stats->log_drop_lines, status->reconstructed_length,
        stats->udp_rx_throttle_enter, stats->udp_rx_throttle_skips,
        status->pio_stalled ? "true" : "false", status->tx_fifo_underruns,
        status->tx_cts_restarts, status->cts_level ? "true" : "false",
        status->cts_edge_count,
        status->cts_restart_enabled ? "true" : "false");
}

static bool append_group_value(char* body, size_t capacity, size_t* used,
                              const char* path,
                              const management_runtime_view_t* view)
{
    if ((strcmp(path, "help") == 0) || (strcmp(path, "discovery") == 0))
    {
        return append_discovery_value(body, capacity, used);
    }
    if (strcmp(path, "status") == 0)
    {
        return append_status_value(body, capacity, used, view);
    }
    if (strcmp(path, "memory") == 0)
    {
        return append_memory_value(body, capacity, used, &view->memory);
    }
    if (strcmp(path, "config") == 0)
    {
        return append_config_value(body, capacity, used, view);
    }
    if (strcmp(path, "net") == 0)
    {
        return append_network_value(body, capacity, used, view);
    }
    if (strcmp(path, "v24") == 0)
    {
        return append_v24_value(body, capacity, used, view);
    }
    if ((strcmp(path, "pins") == 0) || (strcmp(path, "pin") == 0))
    {
        return append_pins_value(body, capacity, used, view);
    }
    return true;
}

static bool append_json_value(char* body, size_t body_capacity, size_t* used,
                              const management_value_t* value)
{
    switch (value->kind)
    {
        case MANAGEMENT_VALUE_NONE:
            return true;
        case MANAGEMENT_VALUE_BOOL:
            return append_format(body, body_capacity, used, ",\"value\":%s",
                                 value->data.boolean ? "true" : "false");

        case MANAGEMENT_VALUE_U16:
            return append_format(body, body_capacity, used, ",\"value\":%u",
                                 (unsigned int)value->data.u16);
        case MANAGEMENT_VALUE_U32:
        case MANAGEMENT_VALUE_SIGNAL_MASK:
            return append_format(body, body_capacity, used, ",\"value\":%lu",
                                 (unsigned long)value->data.u32);
        case MANAGEMENT_VALUE_MODE:
            return append_format(body, body_capacity, used, ",\"value\":\"%s\"",
                                 operating_mode_name(value->data.mode));
        case MANAGEMENT_VALUE_GPIO:
            return append_format(body, body_capacity, used,
                                 ",\"value\":{\"signal\":\"%s\",\"level\":%s}",
                                 gpio_signal_name(value->data.gpio.signal),
                                 value->data.gpio.level ? "true" : "false");
        case MANAGEMENT_VALUE_IPV4:
            return append_format(body, body_capacity, used,
                                 ",\"value\":\"%u.%u.%u.%u\"",
                                 value->data.ipv4[0], value->data.ipv4[1],
                                 value->data.ipv4[2], value->data.ipv4[3]);
        case MANAGEMENT_VALUE_IPV4_CIDR:
            return append_format(body, body_capacity, used,
                                 ",\"value\":\"%u.%u.%u.%u/%u\"",
                                 value->data.ipv4_cidr.address[0],
                                 value->data.ipv4_cidr.address[1],
                                 value->data.ipv4_cidr.address[2],
                                 value->data.ipv4_cidr.address[3],
                                 value->data.ipv4_cidr.prefix_length);
        case MANAGEMENT_VALUE_TEXT:
            return append_format(body, body_capacity, used, ",\"value\":\"%s\"",
                                 value->data.text);
        default:
            return false;
    }
}

bool management_http_format_response(const char* canonical_path,
                                     const management_response_t* response,
                                     char* output, size_t output_capacity,
                                     size_t* output_length)
{
    if ((canonical_path == NULL) || (response == NULL) || (output == NULL) ||
        (output_length == NULL))
    {
        return false;
    }

    const int header_written = snprintf(
        output, output_capacity,
        "HTTP/1.1 %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 0000\r\n"
        "Connection: close\r\n\r\n",
        http_status(response->result));
    if ((header_written < 0) || ((size_t)header_written >= output_capacity))
    {
        return false;
    }
    char* content_length_digits = strstr(output, "Content-Length: ");
    if (content_length_digits == NULL)
    {
        return false;
    }
    content_length_digits += strlen("Content-Length: ");

    const size_t body_start = (size_t)header_written;
    size_t used = body_start;
    const bool success = ((response->result == MANAGEMENT_RESULT_OK) ||
                          (response->result == MANAGEMENT_RESULT_ACCEPTED)) ? true : false;
    if (success)
    {
        if (!append_format(output, output_capacity, &used,
                           "{\"result\":\"%s\",\"path\":\"%s\"",
                           management_result_name(response->result), canonical_path) ||
            ((response->value.kind == MANAGEMENT_VALUE_NONE)
                 ? !append_group_value(output, output_capacity, &used,
                                       canonical_path, &response->snapshot)
                 : !append_json_value(output, output_capacity, &used,
                                      &response->value)) ||
            !append_format(output, output_capacity, &used, "}"))
        {
            return false;
        }
    }
    else if (!append_format(output, output_capacity, &used,
                            "{\"result\":\"error\",\"error\":\"%s\"}",
                            management_result_name(response->result)))
    {
        return false;
    }

    const size_t body_length = used - body_start;
    if (body_length > 9999U)
    {
        return false;
    }
    content_length_digits[0] = (char)('0' + ((body_length / 1000U) % 10U));
    content_length_digits[1] = (char)('0' + ((body_length / 100U) % 10U));
    content_length_digits[2] = (char)('0' + ((body_length / 10U) % 10U));
    content_length_digits[3] = (char)('0' + (body_length % 10U));
    *output_length = used;
    return true;
}
