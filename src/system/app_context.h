

#ifndef APP_CONTEXT_H
#define APP_CONTEXT_H

// Related headers

// Standard library headers
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Library Headers
#include "pico/stdio.h"
#include "pico/time.h"
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"

// Project Headers
#include "drivers/pio_tx_rx_driver.h"
#include "drivers/tx_queue.h"
#include "drivers/w5500_driver.h"
#include "platform/pinmap.h"
#include "protocol/hdlc_common.h"
#include "protocol/hdlc_decoder.h"
#include "protocol/hdlc_sync.h"
#include "system/baudrate_monitor.h"
#include "system/cli_commands.h"
#include "system/cli_usb_cdc.h"
#include "system/common.h"
#include "system/event_queue.h"
#include "system/persistent_config.h"

// Generated headers

typedef struct
{
    config_t persistent_config;
    bool     config_valid;

    UDP_CONFIG_T     local_config;
    UDP_CONFIG_T     destination_config;
    UDP_CONFIG_T     sender_config;
    NETWORK_CONFIG_T net_config;

    V24_CONFIG_T v24_config;

    uint8_t     rx_frame_buffer_data[RX_BUF_SIZE];
    UDP_FRAME_T rx_frame_buffer;
    uint8_t     tx_frame_buffer_data[TX_BUF_SIZE];
    UDP_FRAME_T tx_frame_buffer;

    uint8_t                 reconstructed_frame_buffer[RX_HDLC_SYNC_MAX_BUFFER_SIZE];
    HDLC_FRAME_T            reconstructed_frame;
    HDLC_SYNC_ACCUMULATOR_T accumulator;

    uint8_t    tx_queue_buffer[TX_FRAME_QUEUE_SIZE * sizeof(TX_QUEUE_ENTRY_T)];
    TX_QUEUE_T tx_queue;
    // ... anything else the event loop touches
} app_ctx_t;

void init_app(app_ctx_t* app, config_t* persistent_config);

#endif /* APP_CONTEXT_H */
