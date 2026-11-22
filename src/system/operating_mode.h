#ifndef OPERATING_MODE_H
#define OPERATING_MODE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    OPERATING_MODE_BRIDGE = 0,
    OPERATING_MODE_GPIO   = 1,
} operating_mode_t;

#define OPERATING_MODE_DEFAULT OPERATING_MODE_BRIDGE

bool operating_mode_is_valid(operating_mode_t mode);
operating_mode_t operating_mode_from_persisted(uint32_t stored_mode);
const char* operating_mode_name(operating_mode_t mode);
bool operating_mode_change_allowed(operating_mode_t current_mode,
                                   operating_mode_t requested_mode,
                                   bool frame_active,
                                   bool queue_empty);

#endif /* OPERATING_MODE_H */
