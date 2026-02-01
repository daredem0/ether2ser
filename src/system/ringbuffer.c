




// Related headers
#include "ringbuffer.h"

// Standard library headers
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

// Library Headers
#include "pico/stdio.h"
#include "pico/time.h"
#include "wizchip_conf.h"
#include "wizchip_qspi_pio.h"

// Project Headers
#include "system/cli_commands.h"
#include "system/event_queue.h"
#include "system/cli_usb_cdc.h"
#include "system/baudrate_monitor.h"
#include "drivers/w5500_driver.h"
#include "drivers/pio_tx_rx_driver.h"
#include "platform/pinmap.h"


int RbInit(Ringbuffer *bufferStruct, void *bufferPointer, size_t capacity, size_t itemSizeInByte)
{
    bufferStruct->buffer = bufferPointer;
    if (bufferStruct->buffer == NULL)
    {
        return -1;
    }
    bufferStruct->bufferEnd = (uint8_t *)bufferStruct->buffer + capacity * itemSizeInByte;
    bufferStruct->capacity = capacity;
    bufferStruct->count = 0U;
    bufferStruct->itemSizeInByte = itemSizeInByte;
    bufferStruct->head = bufferStruct->buffer;
    bufferStruct->tail = bufferStruct->buffer;
    memset(bufferStruct->buffer, 0U, capacity * itemSizeInByte);
    return 0;
}

int RbPushBack(Ringbuffer *bufferStruct, const void *element)
{
    if (bufferStruct->count == bufferStruct->capacity)
    {
        return -1;
    }
    memcpy(bufferStruct->head, element, bufferStruct->itemSizeInByte);
    bufferStruct->head = (uint8_t *)bufferStruct->head + bufferStruct->itemSizeInByte;
    if (bufferStruct->head == bufferStruct->bufferEnd)
    {
        bufferStruct->head = bufferStruct->buffer;
    }
    ++bufferStruct->count;
    return 0;
}

int RbPopFront(Ringbuffer *bufferStruct, void *element)
{
    if (bufferStruct->count == 0U)
    {
        return -1;
    }
    memcpy(element, bufferStruct->tail, bufferStruct->itemSizeInByte);
    bufferStruct->tail = (uint8_t *)bufferStruct->tail + bufferStruct->itemSizeInByte;
    if (bufferStruct->tail == bufferStruct->bufferEnd)
    {
        bufferStruct->tail = bufferStruct->buffer;
    }
    --bufferStruct->count;
    return 0;
}

void RbPushBackWrap(Ringbuffer *bufferStruct, const void *element)
{
    if (bufferStruct->count == bufferStruct->capacity)
    {
        // Buffer is full, overwrite the oldest element
        bufferStruct->tail = (uint8_t *)bufferStruct->tail + bufferStruct->itemSizeInByte;
        if (bufferStruct->tail == bufferStruct->bufferEnd)
        {
            bufferStruct->tail = bufferStruct->buffer; // Wrap around
        }
    }
    else
    {
        // Increment count only if the buffer was not full
        ++bufferStruct->count;
    }

    // Add the new element at the head position
    memcpy(bufferStruct->head, element, bufferStruct->itemSizeInByte);
    bufferStruct->head = (uint8_t *)bufferStruct->head + bufferStruct->itemSizeInByte;
    if (bufferStruct->head == bufferStruct->bufferEnd)
    {
        bufferStruct->head = bufferStruct->buffer; // Wrap around
    }
}
