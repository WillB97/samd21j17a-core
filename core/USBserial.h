#pragma once

#include <stdint.h>
#include "RingBuffer.h"
#include "USB-CDC.h"

#define USB_SERIAL_BUFFER_LENGTH 64

class USBserial {
public:
    USBserial();
    ~USBserial();

    // returns bytes added to buffer
    uint8_t write(const char* data, uint8_t len);
    uint8_t write(uint8_t c);

    // returns bytes retrieved
    uint8_t read(char* buffer, uint8_t max_len);

    // returns character
    uint8_t read_char();

    bool writeBufFull();
    bool readBufFull();

    // data available
    bool available();
    // port open
    bool isOpen();

    uint32_t baudrate();
    bool DTR();

    uint8_t* _receive_data_cb(uint8_t* buffer, uint8_t len, uint8_t* new_len);
    uint8_t* _send_data_cb(uint8_t tx_len, uint8_t* new_len);

private:
    RingBuffer<USB_SERIAL_BUFFER_LENGTH, BUFFER_USB_TX_ALIGN> tx_buffer;
    RingBuffer<USB_SERIAL_BUFFER_LENGTH, BUFFER_USB_RX_ALIGN> rx_buffer;
    bool receiveDMAInProgress = false;
    bool transmitDMAInProgress = false;

    uint8_t __irq_status = 0;
    inline void pauseInterrupts() {__irq_status=__get_PRIMASK(); __disable_irq();}
    inline void resumeInterrupts() {if(!__irq_status){__enable_irq();}}
};

extern USBserial usbserial;

extern "C" {
    uint8_t* usbserial_receive_data_cb(uint8_t* buffer, uint8_t len, uint8_t* new_len);
    uint8_t* usbserial_send_data_cb(uint8_t tx_len, uint8_t* new_len);
}
