

// Related headers

// Standard library headers
#include <stdio.h>
#include <stdint.h>

// Library Headers
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "pico/time.h"
#include "pico/types.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "wizchip_conf.h"
#include "w5500.h"
#include "socket.h"

// Project Headers

// Generated headers

#define W5500_SPI spi0
#define W5500_PIN_MISO 16
#define W5500_PIN_CS 17
#define W5500_PIN_SCK 18
#define W5500_PIN_MOSI 19
#define W5500_PIN_RST 20

// W5500 SPI functions
static inline void wiz_cs_select(void) { gpio_put(W5500_PIN_CS, 0); }
static inline void wiz_cs_deselect(void) { gpio_put(W5500_PIN_CS, 1); }

// ioLibrary expects these signatures:
static uint8_t wiz_spi_readbyte(void)
{
    uint8_t rx;
    // Write dummy 0x00 while reading
    spi_read_blocking(W5500_SPI, 0x00, &rx, 1);
    return rx;
}

static void wiz_spi_writebyte(uint8_t tx)
{
    spi_write_blocking(W5500_SPI, &tx, 1);
}

static void wiz_spi_readburst(uint8_t *pBuf, uint16_t len)
{
    spi_read_blocking(W5500_SPI, 0x00, pBuf, len);
}

static void wiz_spi_writeburst(uint8_t *pBuf, uint16_t len)
{
    spi_write_blocking(W5500_SPI, pBuf, len);
}

static void w5500_reset_pulse(void)
{
    gpio_put(W5500_PIN_RST, 0);
    sleep_ms(5);
    gpio_put(W5500_PIN_RST, 1);
    // give W5500 time to come out of reset
    sleep_ms(150);
}

static void w5500_port_init(void)
{
    // GPIO for CS/RST
    gpio_init(W5500_PIN_CS);
    gpio_set_dir(W5500_PIN_CS, GPIO_OUT);
    wiz_cs_deselect();

    gpio_init(W5500_PIN_RST);
    gpio_set_dir(W5500_PIN_RST, GPIO_OUT);
    gpio_put(W5500_PIN_RST, 1);

    // SPI pins
    spi_init(W5500_SPI, 10 * 1000 * 1000); // 10 MHz safe starting point
    gpio_set_function(W5500_PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(W5500_PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(W5500_PIN_SCK, GPIO_FUNC_SPI);

    // W5500 uses CS as a normal GPIO (not hardware CS)
    gpio_init(W5500_PIN_CS);
    gpio_set_dir(W5500_PIN_CS, GPIO_OUT);
    wiz_cs_deselect();

    // Register callbacks with ioLibrary
    reg_wizchip_cs_cbfunc(wiz_cs_select, wiz_cs_deselect);
    reg_wizchip_spi_cbfunc(wiz_spi_readbyte, wiz_spi_writebyte);
    reg_wizchip_spiburst_cbfunc(wiz_spi_readburst, wiz_spi_writeburst);

    w5500_reset_pulse();
}

int main(void)
{
    stdio_init_all();
    sleep_ms(2000);
    printf("v24-eth-bridge: hello from RP2040\r\n");
    printf("UDP Echo example\r\n");

    // Setup SPI
    w5500_port_init();

    // Configure W5500 internal socket buffer sizes (2KB per socket is fine for now)
    uint8_t txsize[8] = {2, 2, 2, 2, 2, 2, 2, 2};
    uint8_t rxsize[8] = {2, 2, 2, 2, 2, 2, 2, 2};
    int8_t rc = wizchip_init(txsize, rxsize);
    printf("wizchip_init rc=%d\n", rc);

    // Read VERSIONR (should be 0x04)
    uint8_t ver = WIZCHIP_READ(VERSIONR);
    printf("W5500 VERSIONR=0x%02X (expected 0x04)\n", ver);

    // Touch a symbol so the compiler doesn't optimize everything away
    // Call something that forces linking against the library
    // (doesn't touch hardware because we won't init the chip).
    int8_t s = socket(0, Sn_MR_UDP, 12345, 0);
    printf("socket() returned %d (expected fail without init)\n", (int)s);

    while (true)
    {
        sleep_ms(1000);
    }
}
