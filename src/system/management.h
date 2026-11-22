#ifndef MANAGEMENT_H
#define MANAGEMENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "drivers/gpio_control.h"
#include "system/operating_mode.h"
#include "system/management_status.h"

typedef enum
{
    MANAGEMENT_METHOD_GET = 0,
    MANAGEMENT_METHOD_SET,
    MANAGEMENT_METHOD_ACTION,
} management_method_t;

typedef enum
{
    MANAGEMENT_RESOURCE_DISCOVERY = 0,
    MANAGEMENT_RESOURCE_STATUS,
    MANAGEMENT_RESOURCE_MEMORY,
    MANAGEMENT_RESOURCE_CONFIG,
    MANAGEMENT_RESOURCE_NET,
    MANAGEMENT_RESOURCE_NET_IP_LOCAL,
    MANAGEMENT_RESOURCE_NET_IP_REMOTE,
    MANAGEMENT_RESOURCE_NET_IP_GATEWAY,
    MANAGEMENT_RESOURCE_NET_UDP_PORT_LOCAL,
    MANAGEMENT_RESOURCE_NET_UDP_PORT_REMOTE,
    MANAGEMENT_RESOURCE_V24,
    MANAGEMENT_RESOURCE_V24_BAUDRATE,
    MANAGEMENT_RESOURCE_V24_TXCLOCK,
    MANAGEMENT_RESOURCE_V24_INVERT,
    MANAGEMENT_RESOURCE_V24_CTS_RESTART,
    MANAGEMENT_RESOURCE_SYSTEM_VERSION,
    MANAGEMENT_RESOURCE_SYSTEM_LOGLEVEL,
    MANAGEMENT_RESOURCE_SYSTEM_MODE,
    MANAGEMENT_RESOURCE_PINS,
    MANAGEMENT_RESOURCE_PIN,
    MANAGEMENT_RESOURCE_GPIO,
    MANAGEMENT_RESOURCE_ACTION_SAVE,
    MANAGEMENT_RESOURCE_ACTION_RESET_STATS,
    MANAGEMENT_RESOURCE_ACTION_WIPE,
    MANAGEMENT_RESOURCE_ACTION_REBOOT,
} management_resource_t;

typedef enum
{
    MANAGEMENT_VALUE_NONE = 0,
    MANAGEMENT_VALUE_BOOL,
    MANAGEMENT_VALUE_U16,
    MANAGEMENT_VALUE_U32,
    MANAGEMENT_VALUE_MODE,
    MANAGEMENT_VALUE_GPIO,
    MANAGEMENT_VALUE_IPV4,
    MANAGEMENT_VALUE_IPV4_CIDR,
    MANAGEMENT_VALUE_SIGNAL_MASK,
    MANAGEMENT_VALUE_TEXT,
} management_value_kind_t;

typedef struct
{
    uint8_t address[4];
    uint8_t prefix_length;
} management_ipv4_cidr_t;

typedef struct
{
    gpio_signal_t signal;
    bool          level;
} management_gpio_value_t;

typedef struct
{
    management_value_kind_t kind;
    union
    {
        bool                   boolean;
        uint16_t               u16;
        uint32_t               u32;
        operating_mode_t       mode;
        management_gpio_value_t gpio;
        uint8_t                ipv4[4];
        management_ipv4_cidr_t ipv4_cidr;
        const char*            text;
    } data;
} management_value_t;

typedef struct
{
    management_method_t   method;
    management_resource_t resource;
    management_value_t    value;
} management_request_t;

typedef enum
{
    MANAGEMENT_RESULT_OK = 0,
    MANAGEMENT_RESULT_ACCEPTED,
    MANAGEMENT_RESULT_NOT_FOUND,
    MANAGEMENT_RESULT_READ_ONLY,
    MANAGEMENT_RESULT_INVALID_VALUE,
    MANAGEMENT_RESULT_CONFLICT,
    MANAGEMENT_RESULT_BUSY,
    MANAGEMENT_RESULT_TOO_LARGE,
    MANAGEMENT_RESULT_INTERNAL_ERROR,
} management_result_t;

typedef enum
{
    MANAGEMENT_EFFECT_NONE = 0,
    MANAGEMENT_EFFECT_SET,
    MANAGEMENT_EFFECT_SAVE,
    MANAGEMENT_EFFECT_RESET_STATS,
    MANAGEMENT_EFFECT_WIPE,
    MANAGEMENT_EFFECT_REBOOT,
} management_effect_kind_t;

typedef struct
{
    management_effect_kind_t kind;
    management_resource_t    resource;
    management_value_t       value;
    bool                     persist;
    bool                     reboot;
} management_effect_t;

typedef struct
{
    const char*      firmware_version;
    operating_mode_t operating_mode;
    bool             frame_active;
    bool             queue_empty;
    uint32_t         gpio_levels;
    management_ipv4_cidr_t local_ip;
    uint8_t          remote_ip[4];
    uint8_t          gateway[4];
    uint16_t         local_udp_port;
    uint16_t         remote_udp_port;
    uint32_t         v24_baudrate;
    uint32_t         v24_invert_mask;
    bool             v24_external_clock;
    bool             v24_cts_restart;
    uint32_t         log_level;
    management_status_snapshot_t status;
    management_memory_snapshot_t memory;
} management_runtime_view_t;

typedef struct
{
    management_result_t result;
    management_value_t  value;
    management_effect_t effect;
    bool                changed;
    management_runtime_view_t snapshot;
} management_response_t;

management_result_t management_execute(const management_request_t* request,
                                       const management_runtime_view_t* runtime,
                                       management_response_t* response);
const char* management_result_name(management_result_t result);

#endif /* MANAGEMENT_H */
