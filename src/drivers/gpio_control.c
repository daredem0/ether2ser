#include "drivers/gpio_control.h"

#include <string.h>

#include "platform/pinmap.h"

static const gpio_signal_info_t gpio_signals[] = {
    {GPIO_SIGNAL_TXD, "txd", V24_TXD, GPIO_SIGNAL_OUTPUT, false},
    {GPIO_SIGNAL_RXD, "rxd", V24_RXD, GPIO_SIGNAL_INPUT, false},
    {GPIO_SIGNAL_RTS, "rts", V24_RTS, GPIO_SIGNAL_OUTPUT, false},
    {GPIO_SIGNAL_CTS, "cts", V24_CTS, GPIO_SIGNAL_INPUT, false},
    {GPIO_SIGNAL_DTR, "dtr", V24_DTR, GPIO_SIGNAL_OUTPUT, false},
    {GPIO_SIGNAL_DSR, "dsr", V24_DSR, GPIO_SIGNAL_INPUT, false},
    {GPIO_SIGNAL_DCD, "dcd", V24_DCD, GPIO_SIGNAL_INPUT, false},
    {GPIO_SIGNAL_TX_ACTIVE, "tx_active", V24_TX_ACTIVE, GPIO_SIGNAL_OUTPUT, false},
    {GPIO_SIGNAL_TCK, "tck", V24_TXC_DTE, GPIO_SIGNAL_OUTPUT, false},
    {GPIO_SIGNAL_RCK, "rck", V24_RXC, GPIO_SIGNAL_INPUT, false},
    {GPIO_SIGNAL_XCK, "xck", V24_TXC_DCE, GPIO_SIGNAL_INPUT, false},
    {GPIO_SIGNAL_W550_RESET, "w550_reset", V24_STATUS_LED, GPIO_SIGNAL_OUTPUT, true},
};

size_t gpio_control_signal_count(void)
{
    return sizeof(gpio_signals) / sizeof(gpio_signals[0]);
}

const gpio_signal_info_t* gpio_control_signal_at(size_t index)
{
    return index < gpio_control_signal_count() ? &gpio_signals[index] : NULL;
}

const gpio_signal_info_t* gpio_control_find_signal(const char* name)
{
    if (name == NULL)
    {
        return NULL;
    }

    for (size_t i = 0U; i < gpio_control_signal_count(); i++)
    {
        if (strcmp(gpio_signals[i].name, name) == 0)
        {
            return &gpio_signals[i];
        }
    }
    return NULL;
}

bool gpio_control_manual_write_allowed(operating_mode_t mode, gpio_signal_t signal)
{
    if (mode != OPERATING_MODE_GPIO)
    {
        return false;
    }

    for (size_t i = 0U; i < gpio_control_signal_count(); i++)
    {
        const gpio_signal_info_t* info = &gpio_signals[i];
        if (info->signal == signal)
        {
            return info->direction == GPIO_SIGNAL_OUTPUT && !info->reserved;
        }
    }
    return false;
}
