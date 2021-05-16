#include "USBserial.h"

USBserial::USBserial() {
    usbserial_set_tx_callback(usbserial_send_data_cb);
    usbserial_set_rx_callback(usbserial_receive_data_cb);
}
USBserial::~USBserial() {
    // remove callbacks
    usbserial_set_tx_callback(NULL);
    usbserial_set_rx_callback(NULL);
}


uint8_t USBserial::write(const char* data, uint8_t len) {
    pauseInterrupts();
    uint8_t len_stored = tx_buffer.store(data, len);
    // if no running tx transfer start one
    if (!transmitDMAInProgress) {
        usbserial_run_tx_callback(0);
    }
    resumeInterrupts();
    return len_stored;
}
uint8_t USBserial::write(uint8_t c) {
    pauseInterrupts();
    uint8_t len = tx_buffer.store(c);
    // if no running tx transfer start one
    if (!transmitDMAInProgress) {
        usbserial_run_tx_callback(0);
    }
    resumeInterrupts();
    return len;
}

uint8_t USBserial::read(char* buffer, uint8_t max_len) {
    pauseInterrupts();
    uint8_t len = rx_buffer.read((uint8_t*)buffer, max_len);
    // if no running rx transfer start one
    if (!receiveDMAInProgress) {
        usbserial_run_rx_callback(0);
    }
    resumeInterrupts();
    return len;
}

uint8_t USBserial::read_char() {
    pauseInterrupts();
    uint8_t len = rx_buffer.read_char();
    // if no running rx transfer start one
    if (!receiveDMAInProgress) {
        usbserial_run_rx_callback(0);
    }
    resumeInterrupts();
    return len;
}

bool USBserial::writeBufFull() {
    return tx_buffer.isFull();
}
bool USBserial::readBufFull() {
    return rx_buffer.isFull();
}

// data available
bool USBserial::available() {
    return !rx_buffer.isEmpty();
}
// port open
bool USBserial::isOpen() {
    return DTR();
}

uint32_t USBserial::baudrate() {
    return usbserial_get_baudrate();
}
bool USBserial::DTR() {
    return usbserial_get_line_info() & CDC_LINESTATE_DTR_MASK;
}

// Receive buffer DMA callback
// Called when the USB serial endpoint completes a host to device transfer to inform the ring buffer
// of the new data and optionally trigger another transfer while there is space in the buffer.
uint8_t* USBserial::_receive_data_cb(uint8_t* buffer, uint8_t len, uint8_t* new_len) {
    if (len > 0) {
        // update the headPtr to reflect the data added by this DMA
        rx_buffer.completeDirectWrite(len);
    }
    if (!rx_buffer.isFull()) { // while there's space in the buffer trigger another transfer
        receiveDMAInProgress = true;
        return rx_buffer.prepareDirectWrite(new_len);
    } else {
        receiveDMAInProgress = false;
        // inform the handler that another transfer is not required
        return NULL;
    }
}

// Transmit buffer DMA callback
// Called when the USB serial endpoint completes a device to host transfer to inform the ring buffer
// of the data that has been sent and optionally trigger another transfer while the buffer still contains data.
uint8_t* USBserial::_send_data_cb(uint8_t tx_len, uint8_t* new_len) {
    if (tx_len > 0) {
        // update the tailPtr to reflect the data sent by this DMA
        tx_buffer.completeDirectRead(tx_len);
    }
    if (!tx_buffer.isEmpty()) { // while the buffer still contains data trigger another transfer
        uint8_t* tx_head = tx_buffer.prepareDirectRead(new_len);
        if (*new_len) {
            transmitDMAInProgress = true;
            return tx_head;
        } else {
            transmitDMAInProgress = false;
            // inform the handler that another transfer is not required
            return NULL;
        }
    } else {
        transmitDMAInProgress = false;
        // inform the handler that another transfer is not required
        return NULL;
    }
}

USBserial usbserial;

uint8_t* usbserial_receive_data_cb(uint8_t* buffer, uint8_t len, uint8_t* new_len) {
    return usbserial._receive_data_cb(buffer, len, new_len);
}
uint8_t* usbserial_send_data_cb(uint8_t tx_len, uint8_t* new_len) {
    return usbserial._send_data_cb(tx_len, new_len);
}

