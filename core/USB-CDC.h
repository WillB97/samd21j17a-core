#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define ACM_SUPPORT_LINE_CODING 0x02
#define ACM_SUPPORT_LINE_BREAK  0x04

#define CDC_LINESTATE_DTR_MASK  0x01 // Data Terminal Ready
#define CDC_LINESTATE_RTS_MASK  0x02 // Ready to Send


#ifdef __cplusplus
}
#endif
