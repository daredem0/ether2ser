


#ifndef HDLC_COMMON_H
#define HDLC_COMMON_H

#include <stdint.h>
#include <stddef.h>

uint16_t hdlc_crc16(const uint8_t* payload, size_t num_bytes);
#endif /* HDLC_COMMON_H */
