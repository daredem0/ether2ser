


// Related headers
#include "gpio_driver.h"

// Standard library headers
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

// Library Headers
#include "pico/stdio.h"
#include "pico/time.h"
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"

// Project Headers
#include "system/cli_commands.h"
#include "system/event_queue.h"
#include "system/cli_usb_cdc.h"
#include "system/baudrate_monitor.h"
#include "drivers/w5500_driver.h"
#include "drivers/pio_tx_rx_driver.h"
#include "platform/pinmap.h"
#include "protocol/hdlc_common.h"
#include "drivers/tx_queue.h"
#include "system/common.h"
#include "protocol/hdlc_sync.h"
#include "protocol/hdlc_decoder.h"

// Generated headers

V24_PIN_T outputs[] = {
    V24_RTS,
    V24_TXD,
    V24_DTR,
    V24_TXC_DTE,
    V24_TXC_DCE
};

V24_PIN_T inputs[] = {
    V24_DCD,
    V24_DSR,
    V24_CTS,
    V24_RXD,
    V24_RXC
};



V24_POLARITIES_T init_polarities(void) {
    return (V24_POLARITIES_T){
        .tx_polarities = {
            .txd_inverted = false,
            .txc_inverted = false,
            .cts_inverted = true,
            .rts_inverted = false,
            .dtr_inverted = false,
        },
        .rx_polarities = {
            .rxd_inverted = false,
            .rxc_inverted = false,
            .dcd_inverted = false,
        }
    };
}

void init_pins(void){
    for(size_t i = 0; i < ARRAY_LEN(inputs); i++) {
        gpio_init(inputs[i]);
        gpio_set_dir(inputs[i], GPIO_IN);
        gpio_pull_down(inputs[i]);
    }

    for(size_t i = 0; i < ARRAY_LEN(outputs); i++) {
        gpio_init(outputs[i]);
        gpio_set_dir(outputs[i], GPIO_OUT);
    }
}
