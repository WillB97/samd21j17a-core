#pragma once

#include <stdint.h>
#include "generic.h"
#include <string.h>  // Required for memcpy


#ifdef __cplusplus

#define BUFFER_USB_RX_ALIGN 0x1
#define BUFFER_USB_TX_ALIGN 0x2

template <uint8_t N, uint8_t align=0> class RingBuffer {
protected:
    // Alignment required for DMA from USB
    __attribute__((__aligned__(4))) uint8_t _buffer[N];
    // if rx alignment is needed add secondary aligned buffer for DMA to write to, set to size=1 otherwise
    __attribute__((__aligned__(4))) uint8_t _rx_buffer[((align & BUFFER_USB_RX_ALIGN)?8:1)];
    uint8_t* volatile headPtr;  // points to the next slot to write data to
    uint8_t* volatile tailPtr;  // points to the next slot to read data from
    uint8_t* volatile endPtr;   // points to the element beyond the end of the buffer

    inline void wrap_headPtr();
    inline void wrap_tailPtr();

    uint8_t __irq_status = 0;
    inline void pauseInterrupts() {__irq_status=__get_PRIMASK(); __disable_irq();}
    inline void resumeInterrupts() {if(!__irq_status){__enable_irq();}}

public:
    RingBuffer();

    uint8_t store(const char* buffer, uint8_t len);
    uint8_t store(const uint8_t c);
    uint8_t read_char();
    uint8_t read(uint8_t* buffer, uint8_t max_len);
    void clear();

    uint8_t usedSpace();
    uint8_t availableSpace();
    bool isFull();
    bool isEmpty();

    /// Helper methods for using the buffer with DMA,
    /// only valid if all data in the same direction uses DMA
    // returns pointer to the start of the writeable sub-buffer
    uint8_t* prepareDirectWrite(uint8_t* len);
    // returns pointer to the start of the readable sub-buffer
    uint8_t* prepareDirectRead(uint8_t* len);

    void completeDirectWrite(uint8_t len);
    void completeDirectRead(uint8_t len);
};


template <uint8_t N, uint8_t align> RingBuffer<N,align>::RingBuffer() {
    // endPtr points to the element beyond the end of the buffer
    endPtr = _buffer + N;
    clear();
}

template <uint8_t N, uint8_t align> inline void RingBuffer<N,align>::wrap_headPtr() {
    if (headPtr >= endPtr) {
        // head moved off the end of the buffer, reset it to the beginning
        headPtr = _buffer;
    }
}
template <uint8_t N, uint8_t align> inline void RingBuffer<N,align>::wrap_tailPtr() {
    if (tailPtr >= endPtr) {
        // tail moved off the end of the buffer, reset it to the beginning
        tailPtr = _buffer;
    }
}

template <uint8_t N, uint8_t align> uint8_t RingBuffer<N,align>::store(const char* buffer, uint8_t len) {
    uint8_t total_transfered = 0;

    pauseInterrupts();
    // create a copy of the buffer pointer since the local copy is not constant but the data pointed at is
    uint8_t* bufferPtr = (uint8_t*)buffer;
    // calculate max length from requested and space available
    uint8_t max_len = min(len, availableSpace());

    // handle splitting on end pointer
    if ((headPtr + max_len) >= endPtr) {
        uint8_t prewrap_len = (endPtr - 1) - headPtr;
        memcpy(headPtr, bufferPtr, prewrap_len);

        // pointer has reached the end of the buffer, reset to start
        headPtr = _buffer;
        // shift the buffer pointer up to the remaining data to copy
        bufferPtr += prewrap_len;
        total_transfered = prewrap_len;
        // get new remaining space
        max_len -= total_transfered;
    }
    memcpy(headPtr, bufferPtr, max_len);
    headPtr += max_len;
    total_transfered += max_len;

    resumeInterrupts();
    return total_transfered;
}
template <uint8_t N, uint8_t align> uint8_t RingBuffer<N,align>::store(const uint8_t c) {
    pauseInterrupts();

    if (isFull()) {
        resumeInterrupts();
        return 0;
    }

    // insert char and step head
    *(headPtr++) = c;
    wrap_headPtr();

    resumeInterrupts();
    return 1;
}

template <uint8_t N, uint8_t align> uint8_t RingBuffer<N,align>::read_char() {
    pauseInterrupts();

    if (isEmpty()) {
        resumeInterrupts();
        /// TODO: what should we return when there isn't anything
        return 0;
    }

    // pop char and step tail
    uint8_t c = *(tailPtr++);
    wrap_tailPtr();

    resumeInterrupts();
    return c;
}
template <uint8_t N, uint8_t align> uint8_t RingBuffer<N,align>::read(uint8_t* buffer, uint8_t max_len) {
    uint8_t total_transfered = 0;

    pauseInterrupts();
    // calculate max length from requested and available data
    uint8_t total_len = min(max_len, usedSpace());

    // handle splitting on end pointer
    if ((tailPtr + total_len) >= endPtr) {
        uint8_t prewrap_len = (endPtr - 1) - tailPtr;
        memcpy(buffer, tailPtr, prewrap_len);

        // pointer has reached the end of the buffer, reset to start
        tailPtr = _buffer;
        // shift the buffer pointer up to where the rest of the data will be copied
        // note: we are only moving our local copy of the buffer pointer,
        // the full buffer will be available once the function returns
        buffer += prewrap_len;
        total_transfered = prewrap_len;
        // get new remaining space
        total_len -= total_transfered;
    }
    memcpy(buffer, tailPtr, total_len);
    tailPtr += total_len;
    total_transfered += total_len;

    resumeInterrupts();
    return total_transfered;
}

template <uint8_t N, uint8_t align> void RingBuffer<N,align>::clear() {
    pauseInterrupts();
    headPtr = _buffer;
    tailPtr = _buffer;
    resumeInterrupts();
}

template <uint8_t N, uint8_t align> uint8_t RingBuffer<N,align>::usedSpace() {
    int16_t ptr_diff = headPtr - tailPtr;

    // account for wrapping at end pointer
    if (ptr_diff < 0) {
        ptr_diff = N + ptr_diff;
    }
    return (uint8_t)(ptr_diff & 0xFF);
}
template <uint8_t N, uint8_t align> uint8_t RingBuffer<N,align>::availableSpace() {
    // at full the pointers are adjacent so 1 slot is always unusable
    return (N - usedSpace() - 1);
}

template <uint8_t N, uint8_t align> bool RingBuffer<N,align>::isFull() {
    return (!availableSpace());
}
template <uint8_t N, uint8_t align> bool RingBuffer<N,align>::isEmpty() {
    return (headPtr == tailPtr);
}

// these functions are not protected against interrupts since they should only be used by a single interrupt
template <uint8_t N, uint8_t align> uint8_t* RingBuffer<N,align>::prepareDirectWrite(uint8_t* len) {
    // calculate maximum contiguous write length
    uint8_t max_len = min((endPtr - headPtr), availableSpace());

    // if the head is already aligned nothing changes
    if ((align & BUFFER_USB_RX_ALIGN) && (reinterpret_cast<uintptr_t>(headPtr) & 0x3)) {
        // when the head is unaligned we use an intermediate buffer so the DMA gets an aligned buffer
        *len = min(max_len, 8);

        return _rx_buffer;
    }

    *len = max_len;

    return headPtr;
}
template <uint8_t N, uint8_t align> uint8_t* RingBuffer<N,align>::prepareDirectRead(uint8_t* len) {
    // calculate maximum contiguous read length
    uint8_t max_len = min((endPtr - tailPtr), usedSpace());

    if (align & BUFFER_USB_TX_ALIGN) {
        if (max_len < 4) {  // if there are less than 4 bytes to send, extra steps are needed
            // push headPtr to the next alignment boundary
            headPtr += 4 - max_len;
            wrap_headPtr();
            *len = max_len;
        } else {
            // restrict length to a multiple of 4 so that all transfers begin on a 32-bit boundary
            *len = (max_len & ~0x3);
        }
    } else {
        *len = max_len;
    }

    return tailPtr;
}

template <uint8_t N, uint8_t align> void RingBuffer<N,align>::completeDirectWrite(uint8_t len) {
    // if the head was aligned we wrote directly to the buffer
    if ((align & BUFFER_USB_RX_ALIGN) && (reinterpret_cast<uintptr_t>(headPtr) & 0x3)) {
        // move data from the intermediate buffer
        memcpy(headPtr, _rx_buffer, len);
    }

    headPtr += len;
    wrap_headPtr();
}
template <uint8_t N, uint8_t align> void RingBuffer<N,align>::completeDirectRead(uint8_t len) {
    if ((align & BUFFER_USB_TX_ALIGN) && (len & 0x3)) {
        // round up to the next alignment boundary
        len = (len & ~0x3) + 4;
    }

    tailPtr += len;
    wrap_tailPtr();
}

#endif
