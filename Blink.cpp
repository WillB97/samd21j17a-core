#include "generic.h"
#include "IO.h"

// PA17 is connected to an LED on most Arduino Zero derivatives

int main( void ) {
    // System is initialised in the Reset_Handler in cortex_handler.c

    // Initialise USB (to be moved to startup.c after testing)
    PORT->Group[PORTA].PMUX[PIN_PA24G_USB_DM/2].reg = PORT_PMUX_PMUXE_G | PORT_PMUX_PMUXO_G;
    PORT->Group[PORTA].PINCFG[PIN_PA24G_USB_DM].bit.PMUXEN = 1;
    PORT->Group[PORTA].PINCFG[PIN_PA25G_USB_DP].bit.PMUXEN = 1;
    usb_init();
    usb_attach();

    // set PA17 to output
    PORT->Group[PORTA].PINCFG[17].reg = 0;
    PORT->Group[PORTA].DIRSET.reg = PORT_PA17;

    // turn PA17 on
    PORT->Group[PORTA].OUTSET.reg = PORT_PA17;

    while (1) {
        // sleep for 1 second
        delay(1000);

        // toggle PA17
        PORT->Group[PORTA].OUTTGL.reg = PORT_PA17;
    }

    return 0;
}
