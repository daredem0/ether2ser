

#ifndef HDLC_ENCODER_H
#define HDLC_ENCODER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// HDLC protocol constants
#define HDLC_FLAG_BYTE 0x7E
#define HDLC_ESCAPE_BYTE 0x7D
#define HDLC_ESCAPE_XOR 0x20

// HDLC Frame Structure
typedef struct{
    uint8_t *payload;    // Buffer containing encoded frame
    size_t length;      // Length of encoded frame
    size_t capacity;    // Maximum length of encoded frame
} HDLC_FRAME_T;

bool hdlc_encode(const uint8_t *payload, const size_t payload_length, HDLC_FRAME_T *frame);

#endif /* HDLC_ENCODER_H */
