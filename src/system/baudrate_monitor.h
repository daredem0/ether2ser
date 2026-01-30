

#ifndef BAUDRATE_MONITOR_H
#define BAUDRATE_MONITOR_H

// Related headers

// Standard library headers

// Library Headers

// Project Headers
#include "platform/pinmap.h"

#define BAUDRATE_MONITOR_PIN V24_RXC

void baudrate_estimator_init(V24_PIN_T pin);
// void baudrate_estimator_poll(V24_PIN_T pin);
float baudrate_estimator_get_current_estimation(V24_PIN_T pin);

#endif /* BAUDRATE_MONITOR_H */
