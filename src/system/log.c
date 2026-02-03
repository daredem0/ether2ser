

#include "common.h"

log_level_t current_log_level = LOG_LEVEL_INFO;

void set_loglevel(log_level_t level) {
    current_log_level = level;
}
