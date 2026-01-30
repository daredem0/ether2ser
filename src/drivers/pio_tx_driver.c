


// Related headers
#include "pio_tx_driver.h"

// Standard library headers
#include <stdio.h>

// Library Headers
#include "pico/types.h"
#include "hardware/pio.h"

// Project Headers
#include "platform/pinmap.h"

// Generated headers
#include "tx_clock.pio.h"

static float baud_to_clockdiv(V24_BAUDRATE_T baudrate){
    return 125000000.0f / (2.0f * (float)baudrate);
}

void tx_clock_init(PIO pio, uint pio_sm, V24_BAUDRATE_T baudrate) {
    float clkdiv = baud_to_clockdiv(baudrate);

    printf("TXC: init pio%u sm%u pin%u baud=%u clkdiv=%.6f\r\n",
           (unsigned)pio_get_index(pio),
           (unsigned)pio_sm,
           (unsigned)V24_TXC_DTE,
           (unsigned)baudrate,
           (double)clkdiv);

    // Load PIO program
    uint offset = pio_add_program(pio, &tx_clock_program);
    printf("TXC: program offset=%u\r\n", (unsigned)offset);

    // Route GPIO to PIO
    pio_gpio_init(pio, V24_TXC_DTE);
    pio_sm_set_consecutive_pindirs(pio, pio_sm, V24_TXC_DTE, 1, true);

    // Configure state machine
    pio_sm_config config = tx_clock_program_get_default_config(offset);
    sm_config_set_sideset_pins(&config, V24_TXC_DTE);
    sm_config_set_clkdiv(&config, clkdiv);
    pio_sm_init(pio, pio_sm, offset, &config);
    pio_sm_set_enabled(pio, pio_sm, true);
    printf("TXC: enabled\r\n");
}
