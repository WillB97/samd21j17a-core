/*
  Copyright (c) 2021 William Barber.  All right reserved.
  Copyright (c) 2015 Arduino LLC.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <samd.h>
#include <stdint.h>
#include "generic.h"

/** Tick Counter united by ms */
static volatile uint32_t _ulTickCount=0 ;

unsigned long millis( void ) {
  return _ulTickCount ;
}

void delay( unsigned long ms ) {
  if (ms == 0) {
    return;
  }

  uint32_t start = millis();
  uint32_t end = start + ms;
  if (end < start) {
    // Overflow condition: loop up to uint32_max
    while (end < millis()) {
      __asm__ __volatile__("");
    }
  }

  while (end > millis()) {
    __asm__ __volatile__("");
  }
}

#include "Reset.h" // for tickReset()

void SysTick_Handler(void) {
  // Increment tick count each ms
  _ulTickCount++;
  tickReset();
}

#ifdef __cplusplus
}
#endif
