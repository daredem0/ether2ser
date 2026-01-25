// Related headers

// Standard library headers
#include <stdio.h>

// Library Headers
#include "pico/stdio.h"
#include "pico/time.h"
#include "pico/types.h"
#include "hardware/pio.h"

// Project Headers

// Generated headers
#include "led_blink.pio.h"

void start_apu_led_blink(uint pin)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);

    bool on = false;
    while (true)
    {
        on = !on;
        gpio_put(pin, on);
        printf("tick: led=%d\r\n", on ? 1 : 0);
        sleep_ms(500);
    }
}
