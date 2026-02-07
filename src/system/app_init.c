

// Related headers
#include "app_context.h"

// Standard library headers
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

// Library Headers
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"

// Project Headers
#include "drivers/pio_tx_rx_driver.h"
#include "drivers/w5500_driver.h"
#include "protocol/hdlc_common.h"
#include "protocol/hdlc_sync.h"
#include "system/common.h"
#include "system/persistent_config.h"

// Generated headers

void init_app(app_ctx_t* app, config_t* persistent_config)
{
    memset(&app->stats, 0, sizeof(app->stats));
    app->need_prompt             = true;
    app->rx_frame_buffer.payload = app->rx_frame_buffer_data;
    app->rx_frame_buffer.length  = 0;

    app->tx_frame_buffer.payload = app->tx_frame_buffer_data;
    app->tx_frame_buffer.length  = 0;

    if (app->config_valid)
    {
        memcpy(&app->persistent_config, persistent_config, sizeof(config_t));
        set_loglevel(app->persistent_config.log_level);

        app->local_config       = (UDP_CONFIG_T)app->persistent_config.local_config;
        app->destination_config = (UDP_CONFIG_T)app->persistent_config.remote_config;
        app->v24_config         = (V24_CONFIG_T)app->persistent_config.v24_config;
        app->net_config         = (NETWORK_CONFIG_T)app->persistent_config.net_config;
        w5500_set_network(&app->net_config);
        init_v24_config(&app->v24_config, app->v24_config.baudrate);
    }
    else
    {
        // Setup loglevel
        set_loglevel(LOG_LEVEL_DEBUG);
        // Initialize Network Configuration
        app->local_config = (UDP_CONFIG_T){
            .ip_address = DEFAULT_IP_ADDR,
            .port       = DEFAULT_UDP_PORT,
        };
        w5500_set_network_defaults(&app->net_config);
        init_v24_config(&app->v24_config, V24_BAUD_9600);
        app->destination_config = (UDP_CONFIG_T){
            .port = DEFAULT_UDP_PORT,
        };
        memcpy(app->destination_config.ip_address, app->net_config.broadcast_address, 4);
    }

    // Initialize HDLC Sync
    hdlc_sync_acc_init(&app->accumulator, HDLC_FLAG_BYTE);
    app->reconstructed_frame = (HDLC_FRAME_T){.payload  = app->reconstructed_frame_buffer,
                                              .length   = 0,
                                              .capacity = sizeof(app->reconstructed_frame_buffer)};
}
