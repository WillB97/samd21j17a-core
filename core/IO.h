#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// I/O Ports definitions
typedef enum {  // Port_t
    PORTA = 0ul,
    PORTB = 1ul
} Port_t;

typedef enum {  // PinMode_t
    OFF,  // input and output drivers disabled
    IN,
    IN_PULLUP,
    IN_PULLDOWN,
    OUT,
    OUT_ONLY,  // input driver disabled
} PinMode_t;

typedef enum {  // PinMux_t
    PINMUX_DIGITAL = -1,    // pin controlled by PORT
    PINMUX_EXTINT  =  0,    // pin controlled by function A
    PINMUX_ANALOG,          // pin controlled by function B
    PINMUX_SERCOM,          // pin controlled by function C
    PINMUX_SERCOM_ALT,      // pin controlled by function D
    PINMUX_TIMER,           // pin controlled by function E
    PINMUX_TIMER_ALT,       // pin controlled by function F
    PINMUX_COM,             // pin controlled by function G
    PINMUX_AC_GLCK,         // pin controlled by function H
} PinMux_t;

typedef enum {  // ADCRef_t
    ADC_REF_INT1V   = 0x0,
    ADC_REF_INTVCC0 = 0x1,  // VCC/1.48
    ADC_REF_INTVCC1 = 0x2,  // VCC/2
    ADC_REF_AREFA   = 0x3,  // REFA pin
    ADC_REF_AREFB   = 0x3   // REFB pin
} ADCRef_t;

typedef enum {  // DACRef_t
    DAC_REF_INT1V = 0x0,
    DAC_REF_AVCC  = 0x1,  // VCC
    DAC_REF_VREFA = 0x2   // REFA pin
} DACRef_t;

void setPinMode(Port_t port, uint8_t pin, PinMode_t mode);
void setPinMux(Port_t port, uint8_t pin, PinMux_t mux_function);
void writePin(Port_t port, uint8_t pin, uint8_t level);
void togglePin(Port_t port, uint8_t pin);

uint8_t readPin(Port_t port, uint8_t pin);

void setAdcRef(ADCRef_t reference);
void initAdc(uint8_t resolution);
void setAdcChannel(uint8_t channel);  /// TODO: set gain
uint16_t readAdc();

void setDacRef(DACRef_t reference);
void initDac(uint8_t resolution);
void writeDac(uint16_t value);

#ifdef __cplusplus
}
#endif
