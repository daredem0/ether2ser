


// Related headers
#include "pio_tx_driver.h"

// Standard library headers

// Library Headers
#include "pico/types.h"
#include "hardware/pio.h"

// Project Headers

// Generated headers
#include "tx_clock.h"

#define V24_TXC_DTE 5


void tx_clock_init(PIO pio, uint sm) {

    // Load PIO program
    uint offset = pio_add_program(pio, &tx_clock_program);

    // Route GPIO to PIO
    pio_gpio_init(pio, V24_TXC_DTE);
}
