
#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stddef.h>

typedef struct
{
    void *buffer;
    void *bufferEnd;
    size_t capacity;
    size_t count;
    size_t itemSizeInByte;
    void *head;
    void *tail;
} Ringbuffer;

int RbInit(Ringbuffer *bufferStruct, void *bufferPointer, size_t capacity, size_t itemSizeInByte);
int RbPushBack(Ringbuffer *bufferStruct, const void *element);
int RbPopFront(Ringbuffer *bufferStruct, void *element);
void RbPushBackWrap(Ringbuffer *bufferStruct, const void *element);
#endif /* RINGBUFFER_H */
