#ifndef MANAGEMENT_CLI_H
#define MANAGEMENT_CLI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "system/management.h"

#define MANAGEMENT_CLI_MAX_LINE 127U

management_result_t management_cli_parse(const char* line, management_request_t* request,
                                         const char** canonical_path);
management_result_t management_cli_parse_human(const char* line,
                                               management_request_t* request,
                                               const char** canonical_path);
bool management_cli_format_response(uint32_t sequence, const char* canonical_path,
                                    const management_response_t* response,
                                    char* output, size_t output_capacity);
const char* management_cli_help_overview(void);
const char* management_cli_help_usage(management_resource_t resource);

#endif /* MANAGEMENT_CLI_H */
