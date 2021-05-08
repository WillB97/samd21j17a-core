#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <samd.h>

#define _BV(x) (uint32_t)(1<<x)

#define interrupts()    __enable_irq()
#define noInterrupts()  __disable_irq()

// undefine stdlib's abs if encountered
#ifdef abs
#undef abs
#endif // abs

#define abs(x) ((x)>0?(x):-(x))

// I/O Ports definitions
#define PORTA     (0ul)
#define PORTB     (1ul)

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
