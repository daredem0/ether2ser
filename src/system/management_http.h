#ifndef MANAGEMENT_HTTP_H
#define MANAGEMENT_HTTP_H

#include <stddef.h>
#include <stdint.h>

#include "system/management.h"

#define MANAGEMENT_HTTP_REQUEST_CAPACITY 1024U
#define MANAGEMENT_HTTP_RESPONSE_CAPACITY 4096U

typedef enum
{
    MANAGEMENT_HTTP_PARSE_READY = 0,
    MANAGEMENT_HTTP_PARSE_INCOMPLETE,
    MANAGEMENT_HTTP_PARSE_BAD_REQUEST,
    MANAGEMENT_HTTP_PARSE_METHOD_NOT_ALLOWED,
    MANAGEMENT_HTTP_PARSE_NOT_FOUND,
    MANAGEMENT_HTTP_PARSE_TOO_LARGE,
} management_http_parse_result_t;

management_http_parse_result_t management_http_parse_request(
    const uint8_t* data, size_t data_length, management_request_t* request,
    const char** canonical_path);

bool management_http_format_response(const char* canonical_path,
                                     const management_response_t* response,
                                     char* output, size_t output_capacity,
                                     size_t* output_length);

#endif /* MANAGEMENT_HTTP_H */
