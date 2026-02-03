

#ifndef PERSISTENT_CONFIG_H
#define PERSISTENT_CONFIG_H

// Related headers
// Standard library headers
#include <stddef.h>
#include <string.h>

// Library Headers
#include "hardware/flash.h"
#include "hardware/sync.h"

// Project Headers
#include "drivers/gpio_driver.h"
#include "drivers/pio_tx_rx_driver.h"
#include "drivers/w5500_driver.h"
#include "system/common.h"

// Generated headers

typedef struct
{
    uint32_t         magic;   // e.g., 0xCAFEBABE - validate data is real
    uint32_t         version; // for future migrations
    UDP_CONFIG_T     local_config;
    UDP_CONFIG_T     remote_config;
    NETWORK_CONFIG_T net_config;
    V24_CONFIG_T     v24_config;
    log_level_t      log_level;
} config_t;

bool config_read(config_t* cfg);
void config_write(const config_t* cfg);
bool config_is_valid(void);
void dump_config(void);

#endif /* PERSISTENT_CONFIG_H */
