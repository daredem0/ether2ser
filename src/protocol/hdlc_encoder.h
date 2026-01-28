

#ifndef HDLC_ENCODER_H
#define HDLC_ENCODER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "hdlc_common.h"


bool hdlc_encode(const uint8_t *payload, const size_t payload_length, HDLC_FRAME_T *frame);

#endif /* HDLC_ENCODER_H */
