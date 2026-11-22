#ifndef GPIO_CONTROL_H
#define GPIO_CONTROL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "system/operating_mode.h"

typedef enum
{
    GPIO_SIGNAL_TXD = 0,
    GPIO_SIGNAL_RXD,
    GPIO_SIGNAL_RTS,
    GPIO_SIGNAL_CTS,
    GPIO_SIGNAL_DTR,
    GPIO_SIGNAL_DSR,
    GPIO_SIGNAL_DCD,
    GPIO_SIGNAL_TX_ACTIVE,
    GPIO_SIGNAL_TCK,
    GPIO_SIGNAL_RCK,
    GPIO_SIGNAL_XCK,
    GPIO_SIGNAL_W550_RESET,
} gpio_signal_t;

typedef enum
{
    GPIO_SIGNAL_INPUT,
    GPIO_SIGNAL_OUTPUT,
} gpio_signal_direction_t;

typedef struct
{
    gpio_signal_t           signal;
    const char*             name;
    uint8_t                 gpio_num;
    gpio_signal_direction_t direction;
    bool                    reserved;
} gpio_signal_info_t;

size_t gpio_control_signal_count(void);
const gpio_signal_info_t* gpio_control_signal_at(size_t index);
const gpio_signal_info_t* gpio_control_find_signal(const char* name);
bool gpio_control_manual_write_allowed(operating_mode_t mode, gpio_signal_t signal);

#endif /* GPIO_CONTROL_H */
