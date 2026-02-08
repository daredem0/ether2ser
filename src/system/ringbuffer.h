/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/ringbuffer.h
 * Purpose: Ring buffer data structure and API.
 *
 * SPDX-License-Identifier: Apache-2.0
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

/**
 * @brief Generic fixed-size ring buffer state.
 */
typedef struct
{
    /** Start of raw buffer memory. */
    void*  buffer;
    /** End pointer of raw buffer memory. */
    void*  bufferEnd;
    /** Number of storable elements. */
    size_t capacity;
    /** Current number of elements stored. */
    size_t count;
    /** Size in bytes of one element. */
    size_t itemSizeInByte;
    /** Write cursor. */
    void*  head;
    /** Read cursor. */
    void*  tail;
} Ringbuffer;

/**
 * @brief Initialize ring buffer instance.
 * @param bufferStruct Ring buffer object.
 * @param bufferPointer Raw backing storage.
 * @param capacity Number of elements.
 * @param itemSizeInByte Size of each element in bytes.
 * @return 0 on success, negative on error.
 */
int  RbInit(Ringbuffer* bufferStruct, void* bufferPointer, size_t capacity, size_t itemSizeInByte);

/**
 * @brief Push one element at tail/head end.
 * @param bufferStruct Ring buffer object.
 * @param element Pointer to source element.
 * @return 0 on success, negative on overflow/error.
 */
int  RbPushBack(Ringbuffer* bufferStruct, const void* element);

/**
 * @brief Pop one element from front.
 * @param bufferStruct Ring buffer object.
 * @param element Destination element pointer.
 * @return 0 on success, negative on underflow/error.
 */
int  RbPopFront(Ringbuffer* bufferStruct, void* element);

/**
 * @brief Push one element and overwrite oldest entry when full.
 * @param bufferStruct Ring buffer object.
 * @param element Pointer to source element.
 */
void RbPushBackWrap(Ringbuffer* bufferStruct, const void* element);
#endif /* RINGBUFFER_H */
