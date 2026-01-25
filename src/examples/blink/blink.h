

#ifndef EXAMPLES_BLINK_H
#define EXAMPLES_BLINK_H

// Related headers

// Standard library headers
#include <stdio.h>

// Library Headers
#include "pico/stdio.h"
#include "pico/time.h"
#include "pico/types.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"

// Project Headers

// Generated headers
#include "led_blink.pio.h"

void start_pio_led_blink(PIO pio, uint sm, uint pin);
void start_apu_led_blink(uint pin);

#endif /* EXAMPLES_BLINK_H */
