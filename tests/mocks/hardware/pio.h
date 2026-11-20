#ifndef TESTS_MOCKS_HARDWARE_PIO_H
#define TESTS_MOCKS_HARDWARE_PIO_H

#include <stdbool.h>

typedef unsigned int uint;
typedef struct pio_hw* PIO;

extern PIO pio0;

bool pio_sm_is_tx_fifo_full(PIO pio, uint sm);

#endif /* TESTS_MOCKS_HARDWARE_PIO_H */
