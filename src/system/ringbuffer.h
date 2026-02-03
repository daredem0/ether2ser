/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/ringbuffer.h
 * Purpose: Ring buffer data structure and API.
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef RINGBUFFER_H
#define RINGBUFFER_H

// Related headers

// Standard library headers
#include <stddef.h>

// Project Headers

// Generated headers

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
