#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <samd.h>

#define _BV(x) (uint32_t)(1<<x)

#define interrupts()    __enable_irq()
#define noInterrupts()  __disable_irq()
// This string is in flash and part of the bootloader
#define BOOT_SERIAL_NUMBER ((const char *)0x1FC0)

// inline int abs(const int x) {return (x)>0?(x):-(x);}
inline int min(const int x, const int y) {return (x<y)?(x):(y);}
inline int max(const int x, const int y) {return (x>y)?(x):(y);}

// init_clocks.c
void init( void );
void initSercom( void );
void initTCC( void );
void initADC( void );
void initDAC( void );

// delay.c
unsigned long millis( void );
void delay( unsigned long ms );

#ifdef __cplusplus
}
#endif
