

#ifndef PIO_TX_DRIVER_H
#define PIO_TX_DRIVER_H
// Related headers

// Standard library headers

// Library Headers
#include "pico/types.h"
#include "hardware/pio.h"

// Project Headers

// Generated headers

typedef enum{
    V24_BAUD_1200 = 1200,
    V24_BAUD_2400 = 2400,
    V24_BAUD_4800 = 4800,
    V24_BAUD_9600 = 9600,
    V24_BAUD_19200 = 19200,
    V24_BAUD_38400 = 38400,
    V24_BAUD_57600 = 57600,
    V24_BAUD_115200 = 115200
} V24_BAUDRATE_T;

void tx_clock_init(PIO pio, uint pio_sm, V24_BAUDRATE_T baudrate);
bool tx_put(uint8_t data);

#endif /* PIO_TX_DRIVER_H */
