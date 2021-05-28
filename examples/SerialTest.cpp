#include "generic.h"
#include "IO.h"
#include "USBserial.h"

uint8_t iDebug = 0;
uint8_t bDebug[64];
uint8_t iRef = 0;
uint8_t bRef[64];

int main( void ) {
    // System is initialised in the Reset_Handler in cortex_handler.c

    // set PA17 to output
    PORT->Group[PORTA].PINCFG[17].reg = 0;
    PORT->Group[PORTA].DIRSET.reg = PORT_PA17;

    // wait for port to be opened
    while (!usbserial.isOpen());

    // sleep for 1 second while the port is setup
    delay(1000);

    // turn PA17 on
    PORT->Group[PORTA].OUTSET.reg = PORT_PA17;

    usbserial.write("Serial No", 9);
    usbserial.write(':');
    usbserial.write(' ');
    delay(1000);  // pause to send this data in a separate transfer
    usbserial.write(BOOT_SERIAL_NUMBER, strlen(BOOT_SERIAL_NUMBER));

    // turn PA17 off
    PORT->Group[PORTA].OUTCLR.reg = PORT_PA17;

    // let any pending transfers complete
    delay(100);
    uint8_t length;
    char tmp_buffer[64];

    while (1) {
        // receive string and return it
        length = usbserial.read(tmp_buffer, 64);
        usbserial.write(tmp_buffer, length);
    }

    return 0;
}
