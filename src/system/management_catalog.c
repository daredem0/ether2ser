#include "system/management_catalog.h"

#include <string.h>

#define GET_MASK MANAGEMENT_METHOD_MASK(MANAGEMENT_METHOD_GET)
#define SET_MASK MANAGEMENT_METHOD_MASK(MANAGEMENT_METHOD_SET)
#define ACTION_MASK MANAGEMENT_METHOD_MASK(MANAGEMENT_METHOD_ACTION)

static const management_resource_descriptor_t resources[] = {
    {MANAGEMENT_RESOURCE_DISCOVERY, "", GET_MASK, MANAGEMENT_VALUE_NONE, "List resources"},
    {MANAGEMENT_RESOURCE_STATUS, "status", GET_MASK, MANAGEMENT_VALUE_NONE, "Runtime status"},
    {MANAGEMENT_RESOURCE_MEMORY, "memory", GET_MASK, MANAGEMENT_VALUE_NONE, "Memory usage"},
    {MANAGEMENT_RESOURCE_CONFIG, "config", GET_MASK, MANAGEMENT_VALUE_NONE, "Configuration"},
    {MANAGEMENT_RESOURCE_NET, "net", GET_MASK, MANAGEMENT_VALUE_NONE, "Network summary"},
    {MANAGEMENT_RESOURCE_NET_IP_LOCAL, "net.ip.local", GET_MASK | SET_MASK,
     MANAGEMENT_VALUE_IPV4_CIDR, "Local IPv4 address and prefix"},
    {MANAGEMENT_RESOURCE_NET_IP_REMOTE, "net.ip.remote", GET_MASK | SET_MASK,
     MANAGEMENT_VALUE_IPV4, "Remote IPv4 address"},
    {MANAGEMENT_RESOURCE_NET_IP_GATEWAY, "net.ip.gateway", GET_MASK | SET_MASK,
     MANAGEMENT_VALUE_IPV4, "Gateway IPv4 address"},
    {MANAGEMENT_RESOURCE_NET_UDP_PORT_LOCAL, "net.udp.port.local", GET_MASK | SET_MASK,
     MANAGEMENT_VALUE_U16, "Local bridge UDP port"},
    {MANAGEMENT_RESOURCE_NET_UDP_PORT_REMOTE, "net.udp.port.remote", GET_MASK | SET_MASK,
     MANAGEMENT_VALUE_U16, "Remote bridge UDP port"},
    {MANAGEMENT_RESOURCE_V24, "v24", GET_MASK, MANAGEMENT_VALUE_NONE, "V.24 configuration"},
    {MANAGEMENT_RESOURCE_V24_BAUDRATE, "v24.baudrate", GET_MASK | SET_MASK,
     MANAGEMENT_VALUE_U32, "Configured V.24 baudrate"},
    {MANAGEMENT_RESOURCE_V24_TXCLOCK, "v24.txclock", GET_MASK | SET_MASK,
     MANAGEMENT_VALUE_BOOL, "External TX clock selection"},
    {MANAGEMENT_RESOURCE_V24_INVERT, "v24.invert", GET_MASK | SET_MASK,
     MANAGEMENT_VALUE_SIGNAL_MASK, "Inverted V.24 signals"},
    {MANAGEMENT_RESOURCE_V24_CTS_RESTART, "v24.ctsrestart", GET_MASK | SET_MASK,
     MANAGEMENT_VALUE_BOOL, "Restart TX after a CTS edge"},
    {MANAGEMENT_RESOURCE_SYSTEM_VERSION, "system.version", GET_MASK,
     MANAGEMENT_VALUE_TEXT, "Firmware version"},
    {MANAGEMENT_RESOURCE_SYSTEM_LOGLEVEL, "system.loglevel", GET_MASK | SET_MASK,
     MANAGEMENT_VALUE_U32, "Runtime log level"},
    {MANAGEMENT_RESOURCE_SYSTEM_MODE, "system.mode", GET_MASK | SET_MASK,
     MANAGEMENT_VALUE_MODE, "Operating mode"},
    {MANAGEMENT_RESOURCE_PINS, "pins", GET_MASK, MANAGEMENT_VALUE_NONE, "Pin catalog"},
    {MANAGEMENT_RESOURCE_PIN, "pin", GET_MASK, MANAGEMENT_VALUE_TEXT, "Pin detail"},
    {MANAGEMENT_RESOURCE_GPIO, "gpio", GET_MASK | SET_MASK, MANAGEMENT_VALUE_GPIO,
     "GPIO value"},
    {MANAGEMENT_RESOURCE_ACTION_SAVE, "actions.save", ACTION_MASK, MANAGEMENT_VALUE_NONE,
     "Persist configuration"},
    {MANAGEMENT_RESOURCE_ACTION_RESET_STATS, "actions.reset-stats", ACTION_MASK,
     MANAGEMENT_VALUE_NONE, "Reset runtime statistics"},
    {MANAGEMENT_RESOURCE_ACTION_WIPE, "actions.wipe", ACTION_MASK, MANAGEMENT_VALUE_TEXT,
     "Wipe persisted configuration"},
    {MANAGEMENT_RESOURCE_ACTION_REBOOT, "actions.reboot", ACTION_MASK, MANAGEMENT_VALUE_NONE,
     "Reboot device"},
};

size_t management_catalog_count(void)
{
    return sizeof(resources) / sizeof(resources[0]);
}

const management_resource_descriptor_t* management_catalog_at(size_t index)
{
    return index < management_catalog_count() ? &resources[index] : NULL;
}

const management_resource_descriptor_t* management_catalog_find(const char* path)
{
    if (path == NULL)
    {
        return NULL;
    }
    for (size_t i = 0U; i < management_catalog_count(); i++)
    {
        if (strcmp(resources[i].path, path) == 0)
        {
            return &resources[i];
        }
    }
    return NULL;
}
