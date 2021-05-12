#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include "usb.h"
#include "samd/usb_samd.h"
#include "class/cdc/cdc_standard.h"
#include "Reset.h"

// https://cscott.net/usb_dev/data/devclass/usbcdc11.pdf
#define ACM_SUPPORT_LINE_CODING 0x02
#define ACM_SUPPORT_LINE_BREAK  0x04

#define CDC_LINESTATE_DTR_MASK  0x01 // Data Terminal Ready
#define CDC_LINESTATE_RTS_MASK  0x02 // Ready to Send

#define INTERFACE_CDC_CONTROL 0
#define INTERFACE_CDC_DATA 1

#define USB_EP_CDC_NOTIFICATION 0x81
#define USB_EP_CDC_IN           0x82
#define USB_EP_CDC_OUT          0x02

#ifndef min
#define min(x,y) ((x<y)?(x):(y))
#endif // abs

void detectSerialReset(uint32_t dataRate, uint8_t ctrlLineState);

#ifdef __cplusplus
}
#endif
