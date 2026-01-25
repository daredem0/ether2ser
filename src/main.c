// src/main.c
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"

#include "led_blink.pio.h" // generated from src/pio/led_blink.pio

#ifndef LED_PIN
// Keep your existing APU blink pin as-is.
#define LED_PIN 25
#endif

// W55RP20-EVB-PICO onboard USER LED is on GPIO19.
#define PIO_LED_PIN 19

static void start_pio_led_blink(PIO pio, uint sm, uint pin)
{
    // Load PIO program
    uint offset = pio_add_program(pio, &led_blink_program);

    // Route GPIO to PIO
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);

    // Configure state machine
    pio_sm_config c = led_blink_program_get_default_config(offset);
    sm_config_set_set_pins(&c, pin, 1);

    // Slow down the state machine clock so blinking is visible.
    // 125 MHz / 65535 / 64 cycles ≈ 30 Hz toggle rate (15 Hz blink)
    // For slower: increase delay in PIO program [31] -> larger value
    sm_config_set_clkdiv(&c, 65535.0f);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

int main(void)
{
    stdio_init_all();

    // Give the USB CDC a moment to enumerate (harmless even if not using USB)
    sleep_ms(1000);

    printf("v24-eth-bridge: hello from RP2040\r\n");
    printf("PIO blinking GPIO19 (USER LED), APU blinking GPIO%d\r\n", LED_PIN);

    // --- Start PIO blink on onboard USER LED (GPIO19) ---
    start_pio_led_blink(pio0, 0, PIO_LED_PIN);

    // --- Keep your existing APU blink code unchanged ---
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
