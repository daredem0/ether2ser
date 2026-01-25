// src/main.c
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#ifndef LED_PIN
// Many RP2040 boards use GPIO25 for the onboard LED.
// If your W55RP20-EVB-PICO uses a different pin, change this.
#define LED_PIN 25
#endif

int main(void)
{
    stdio_init_all();

    // Give the USB CDC a moment to enumerate (harmless even if not using USB)
    sleep_ms(500);

    printf("v24-eth-bridge: hello from RP2040\r\n");

    // Try to blink an LED (if connected to LED_PIN)
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    bool on = false;
    while (true)
    {
        on = !on;
        gpio_put(LED_PIN, on);
        printf("tick: led=%d\r\n", on ? 1 : 0);
        sleep_ms(500);
    }
}
