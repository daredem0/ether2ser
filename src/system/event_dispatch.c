/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/event_dispatch.c
 * Purpose: Dispatch bounded control-plane events.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#include "system/event_dispatch.h"

#include "wizchip_conf.h"

#include "platform/watchdog.h"
#include "system/app_context.h"
#include "system/common.h"
#include "system/event_queue.h"
#include "system/management_serial.h"
#include "system/persistent_config.h"

void event_dispatch(const event_t* event, app_ctx_t* app)
{
    if ((event == NULL) || (app == NULL))
    {
        return;
    }

    switch (event->type)
    {
        case EV_REBOOT:
            reboot();
            break;
        case EV_CLI_LINE:
        {
            const char* cli_line = NULL;
            if (event_get_payload_ptr(event, 1U, (const void**)&cli_line))
            {
                management_serial_handle_line(app, cli_line);
            }
            app->need_prompt = true;
            break;
        }
        case EV_SAVE_CONFIG:
            LOG_PLAIN("Storing persistent config in flash.\r\n");
            wizchip_getnetinfo(&app->net_config.net_info);
            app->persistent_config.log_level      = get_loglevel();
            app->persistent_config.v24_config     = app->v24_config;
            app->persistent_config.net_config     = app->net_config;
            app->persistent_config.local_config   = app->local_config;
            app->persistent_config.remote_config  = app->destination_config;
            app->persistent_config.operating_mode = app->operating_mode;
            if (config_write(&app->persistent_config))
            {
                LOG_PLAIN("Config stored.\r\n");
            }
            else
            {
                LOG_PLAIN("Config store failed.\r\n");
            }
            app->need_prompt = true;
            break;
        case EV_WIPE_CONFIG:
            LOG_PLAIN("Wiping persistent config in flash.\r\n");
            if (config_wipe())
            {
                LOG_PLAIN("Config wiped.\r\n");
            }
            else
            {
                LOG_PLAIN("Config wipe failed.\r\n");
            }
            app->need_prompt = true;
            break;
        case EV_NONE:
        default:
            break;
    }
}
