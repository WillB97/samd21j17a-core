#include "USB-CDC.h"

USB_ENDPOINTS(3);

// USB serial function prototypes
void usbserial_init();

// USB serial callbacks
// tx_len, new_len -> new_buffer, set new_buffer to NULL to skip next transfer
static uint8_t* (*usbserial_tx_callback)(uint8_t, uint8_t*) = NULL;
// buffer, len, new_len -> new_buffer, set new_buffer to NULL to skip next transfer
static uint8_t* (*usbserial_rx_callback)(uint8_t*, uint8_t, uint8_t*) = NULL;

static uint8_t* usbserial_current_tx_buffer = NULL;
static uint8_t* usbserial_current_rx_buffer = NULL;

static uint8_t usbserial_current_tx_length = 0;


USB_ALIGN const USB_DeviceDescriptor device_descriptor = {
    .bLength = sizeof(USB_DeviceDescriptor),
    .bDescriptorType = USB_DTYPE_Device,

    .bcdUSB                 = 0x0200,  // USB version 2.0
    .bDeviceClass           = CDC_INTERFACE_CLASS,
    .bDeviceSubClass        = USB_CSCP_NoDeviceSubclass,
    .bDeviceProtocol        = USB_CSCP_NoDeviceProtocol,

    .bMaxPacketSize0        = 64,
    .idVendor               = USB_VID,  // VID
    .idProduct              = USB_PID,  // PID
    .bcdDevice              = 0x0001,  // device release number

    .iManufacturer          = 0x01,  // index of string descriptor
    .iProduct               = 0x02,  // index of string descriptor
    .iSerialNumber          = 0x03,  // index of string descriptor

    .bNumConfigurations     = 1
};

typedef struct ConfigDesc {  // struct to hold config below
    USB_ConfigurationDescriptor Config;

    USB_InterfaceDescriptor CDC_control_interface;

    CDC_FunctionalHeaderDescriptor CDC_functional_header;
    CDC_FunctionalACMDescriptor CDC_functional_ACM;
    CDC_FunctionalUnionDescriptor CDC_functional_union;
    USB_EndpointDescriptor CDC_notification_endpoint;

    USB_InterfaceDescriptor CDC_data_interface;
    USB_EndpointDescriptor CDC_out_endpoint;
    USB_EndpointDescriptor CDC_in_endpoint;
}  __attribute__((packed)) ConfigDesc;

USB_ALIGN const ConfigDesc configuration_descriptor = {
    .Config = {  // descriptor of this config
        .bLength = sizeof(USB_ConfigurationDescriptor),
        .bDescriptorType = USB_DTYPE_Configuration,
        .wTotalLength  = sizeof(ConfigDesc),
        .bNumInterfaces = 2,  // number of interfaces in the config
        .bConfigurationValue = 1,  // value to select this config
        .iConfiguration = 0,  // index of string descriptor for the config, 0 is disabled
        .bmAttributes = USB_CONFIG_ATTR_BUSPOWERED,  // use ths field to select bus/self-powered
        .bMaxPower = USB_CONFIG_POWER_MA(200)  // power in 2mA steps
    },
    .CDC_control_interface = {
        .bLength = sizeof(USB_InterfaceDescriptor),
        .bDescriptorType = USB_DTYPE_Interface,  // an interface descriptor
        .bInterfaceNumber = INTERFACE_CDC_CONTROL,  // 0-indexed number of interface
        .bAlternateSetting = 0,  // the default settings of this interface
        .bNumEndpoints = 1,
        .bInterfaceClass = CDC_INTERFACE_CLASS,
        .bInterfaceSubClass = CDC_INTERFACE_SUBCLASS_ACM,
        .bInterfaceProtocol = 0,
        .iInterface = 0,  // index of string descriptor, 0 is disabled
    },
    .CDC_functional_header = {
        .bLength = sizeof(CDC_FunctionalHeaderDescriptor),
        .bDescriptorType = USB_DTYPE_CSInterface,
        .bDescriptorSubtype = CDC_SUBTYPE_HEADER,
        .bcdCDC = 0x0110,  // version of CDC v1.16
    },
    .CDC_functional_ACM = {
        .bLength = sizeof(CDC_FunctionalACMDescriptor),
        .bDescriptorType = USB_DTYPE_CSInterface,
        .bDescriptorSubtype = CDC_SUBTYPE_ACM,
        .bmCapabilities = 0x06,  // supports setting line coding, and receiving break
    },
    .CDC_functional_union = {
        .bLength = sizeof(CDC_FunctionalUnionDescriptor),
        .bDescriptorType = USB_DTYPE_CSInterface,
        .bDescriptorSubtype = CDC_SUBTYPE_UNION,
        // specify the interfaces in this config which will be the master and slave
        .bMasterInterface = INTERFACE_CDC_CONTROL,
        .bSlaveInterface = INTERFACE_CDC_DATA,
    },
    .CDC_notification_endpoint = {
        .bLength = sizeof(USB_EndpointDescriptor),
        .bDescriptorType = USB_DTYPE_Endpoint,  // an endpoint descriptor
        .bEndpointAddress = USB_EP_CDC_NOTIFICATION,  // endpoint address, b7 = dir (1 = in)
        .bmAttributes = (USB_EP_TYPE_INTERRUPT | ENDPOINT_ATTR_NO_SYNC | ENDPOINT_USAGE_DATA),  // interrupt endpoint
        .wMaxPacketSize = 8,
        .bInterval = 0xFF
    },
    .CDC_data_interface = {
        .bLength = sizeof(USB_InterfaceDescriptor),
        .bDescriptorType = USB_DTYPE_Interface,  // an interface descriptor
        .bInterfaceNumber = INTERFACE_CDC_DATA,  // 0-indexed number of interface
        .bAlternateSetting = 0,  // the default settings of this interface
        .bNumEndpoints = 2,
        .bInterfaceClass = CDC_INTERFACE_CLASS_DATA,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface = 0,  // index of string descriptor, 0 is disabled
    },
    .CDC_out_endpoint = {
        .bLength = sizeof(USB_EndpointDescriptor),
        .bDescriptorType = USB_DTYPE_Endpoint,  // an endpoint descriptor
        .bEndpointAddress = USB_EP_CDC_OUT,  // endpoint address, b7 = dir (0 = out)
        .bmAttributes = (USB_EP_TYPE_BULK | ENDPOINT_ATTR_NO_SYNC | ENDPOINT_USAGE_DATA),  // bulk endpoint
        .wMaxPacketSize = 64,
        .bInterval = 0x05  // polling interval in frames (1ms for full speed, 1/8ms for high speed)
    },
    .CDC_in_endpoint = {
        .bLength = sizeof(USB_EndpointDescriptor),
        .bDescriptorType = USB_DTYPE_Endpoint,  // an endpoint descriptor
        .bEndpointAddress = USB_EP_CDC_IN,  // endpoint address, b7 = dir (1 = in)
        .bmAttributes = (USB_EP_TYPE_BULK | ENDPOINT_ATTR_NO_SYNC | ENDPOINT_USAGE_DATA),  // bulk endpoint
        .wMaxPacketSize = 64,
        .bInterval = 0x05  // polling interval in frames (1ms for full speed, 1/8ms for high speed)
    },
};

USB_ALIGN const USB_StringDescriptor language_string = {
    .bLength = USB_STRING_LEN(1),
    .bDescriptorType = USB_DTYPE_String,
    .bString = {USB_LANGUAGE_EN_US},
};

USB_ALIGN CDC_LineEncoding _usbLineInfo = {115200,0,0,8};
bool _usbPendingNewLineInfo = false;
USB_ALIGN uint8_t _usbCtrlLineInfo = 0x00;

/// Callback for a GET_DESCRIPTOR request
uint16_t usb_cb_get_descriptor(uint8_t type, uint8_t index, const uint8_t** ptr) {
    const void* address = NULL;
    uint16_t size    = 0;

    switch (type) {
        case USB_DTYPE_Device:
            address = &device_descriptor;
            size    = sizeof(USB_DeviceDescriptor);
            break;
        case USB_DTYPE_Configuration:
            address = &configuration_descriptor;
            size    = sizeof(ConfigDesc);
            break;
        case USB_DTYPE_String:  // request for a string descriptor
            switch (index) {
                case 0x00:  // request the supported language for the descriptor strings
                    address = &language_string;
                    break;
                case 0x01:
                    address = usb_string_to_descriptor(USB_MANUFACTURER);
                    break;
                case 0x02:
                    address = usb_string_to_descriptor(USB_PRODUCT);
                    break;
                case 0x03:
                    address = usb_string_to_descriptor((char*)BOOT_SERIAL_NUMBER);  // this should be const but the argument isn't
                    break;
            }
            size = (((USB_StringDescriptor*)address))->bLength;
            break;
    }

    *ptr = address;
    return size;
}

/// Callback on reset
void usb_cb_reset(void) {
}

/// Callback for a SET_CONFIGURATION request
bool usb_cb_set_configuration(uint8_t config) {
    if (config <= 1) {  // select config 1
        usbserial_init();
        return true;
    }
    return false;
}

/// Callback when a setup packet is received
void usb_cb_control_setup(void) {
    uint8_t recipient = usb_setup.bmRequestType & USB_REQTYPE_RECIPIENT_MASK;
    uint8_t req_type = usb_setup.bmRequestType & USB_REQTYPE_TYPE_MASK;

    if (req_type == USB_REQTYPE_CLASS) {
        if (recipient == USB_RECIPIENT_INTERFACE) {

            uint8_t interface = usb_setup.wIndex & 0xff;
            // uint8_t entity = usb_setup.wIndex >> 8;
            if (interface == INTERFACE_CDC_CONTROL) {
                switch (usb_setup.bRequest){
                    case CDC_GET_LINE_ENCODING: {
                        memcpy(ep0_buf_in, (uint8_t*)&_usbLineInfo, 7);
                        usb_ep0_in(7);
                        return usb_ep0_out();
                    }

                    case CDC_SET_LINE_ENCODING: {
                        if (usb_setup.wLength) {
                            _usbPendingNewLineInfo = true;
                        }
                        usb_ep0_in(0);
                        return usb_ep0_out();
                    }

                    case CDC_SET_CONTROL_LINE_STATE: {
                        _usbCtrlLineInfo = usb_setup.wValue & 0xff;
                        detectSerialReset(_usbLineInfo.baud_rate, _usbCtrlLineInfo);
                        usb_ep0_in(0);
                        return usb_ep0_out();
                    }

                    case CDC_SEND_BREAK: {
                        /// TODO: do something with this value
                        // uint16_t breakValue = (uint16_t)(usb_setup.wValue);
                        usb_ep0_in(0);
                        return usb_ep0_out();
                    }

                    default:
                        return usb_ep0_stall();
                }
            }
        }
    }

    return usb_ep0_stall();
}

/// Callback on a completion interrupt
void usb_cb_control_in_completion(void) {
}

/// Callback on a completion interrupt
void usb_cb_control_out_completion(void) {
    if (_usbPendingNewLineInfo) {
        uint32_t len = usb_ep_out_length(0x00);
        memcpy((uint8_t*)&_usbLineInfo, ep0_buf_out, min(len, sizeof(_usbLineInfo)));
        detectSerialReset(_usbLineInfo.baud_rate, _usbCtrlLineInfo);
        _usbPendingNewLineInfo = false;
    }
}

/// Callback on a completion interrupt
void usb_cb_completion(void) {
    if (usb_ep_pending(USB_EP_CDC_OUT)) {
        // if a host to device serial transmission was pending
        // run the callback and mark it as completed
        usb_ep_handled(USB_EP_CDC_OUT);
        uint8_t len = (uint8_t)(usb_ep_out_length(USB_EP_CDC_OUT) & 0xFF);
        usbserial_run_rx_callback(len);
    }

    if (usb_ep_pending(USB_EP_CDC_IN)) {
        // if a device to host serial transmission was pending,
        // run the callback and mark it as completed
        usb_ep_handled(USB_EP_CDC_IN);
        usbserial_run_tx_callback(usbserial_current_tx_length);
    }
}

/// Callback for a SET_INTERFACE request
bool usb_cb_set_interface(uint16_t interface, uint16_t new_altsetting) {
    return false;
}

// Reset over serial support
void detectSerialReset(uint32_t dataRate, uint8_t ctrlLineState) {
  // auto-reset into the bootloader is triggered when the port, already open at 1200 bps, is closed.
  // We check DTR state to determine if host port is open.
  if (dataRate == 1200 && (ctrlLineState & CDC_LINESTATE_DTR_MASK) == 0) {
    initiateReset(250);
  } else {
    cancelReset();
  }
}

// ------------------------------------------------------------------------------------------------------------------
// #### USB serial ####
// ------------------------------------------------------------------------------------------------------------------

#ifdef USB_SERIAL_ECHO
uint8_t* usbserial_out_completion(uint8_t* buffer, uint8_t len, uint8_t* new_len);

uint8_t usbserial_active_tx_buf = 0;
USB_ALIGN uint8_t usbserial_buf[4][64];
#endif

// called by the SET_CONFIGURATION callback when the CDC configuration is selected
void usbserial_init() {
    // configure the USB endpoints
    usb_enable_ep(USB_EP_CDC_NOTIFICATION, USB_EP_TYPE_INTERRUPT, 8);
    usb_enable_ep(USB_EP_CDC_OUT, USB_EP_TYPE_BULK, 64);
    usb_enable_ep(USB_EP_CDC_IN, USB_EP_TYPE_BULK, 64);

#ifdef USB_SERIAL_ECHO
    usbserial_set_rx_callback(usbserial_out_completion);
#endif

    // if callbacks configured, run them
    usbserial_run_tx_callback(0);
    usbserial_run_rx_callback(0);
}

// Configure callbacks for USB serial
void usbserial_set_tx_callback(uint8_t* (*new_tx_isr)(uint8_t, uint8_t*)) {
    usbserial_tx_callback = new_tx_isr;
    if (usb_ep_ready(USB_EP_CDC_IN)) { // if endpoint active
        usbserial_run_tx_callback(0);
    }
}
void usbserial_set_rx_callback(uint8_t* (*new_rx_isr)(uint8_t*, uint8_t, uint8_t*)) {
    usbserial_rx_callback = new_rx_isr;
    if (usb_ep_ready(USB_EP_CDC_IN)) { // if endpoint active
        usbserial_run_rx_callback(0);
    }
}

void usbserial_run_tx_callback(uint8_t len){
    if (usbserial_tx_callback) {
        uint8_t* buffer = usbserial_tx_callback(len, &usbserial_current_tx_length);
        usbserial_current_tx_buffer = buffer;
        if (buffer) {
            usb_ep_start_in(USB_EP_CDC_IN, buffer, usbserial_current_tx_length, false); // start a tx transfer
        }
    } else {
        usbserial_current_tx_buffer = NULL;
        usbserial_current_tx_length = 0;
    }
}
void usbserial_run_rx_callback(uint8_t len){
    if (usbserial_rx_callback) {
        uint8_t new_len;
        uint8_t* buffer = usbserial_rx_callback(usbserial_current_rx_buffer, len, &new_len);
        usbserial_current_rx_buffer = buffer;
        if (buffer) {
            usb_ep_start_out(USB_EP_CDC_OUT, buffer, new_len); // start a rx transfer
        }
    } else {
        usbserial_current_rx_buffer = NULL;
    }
}

uint8_t usbserial_get_line_info() {
    return _usbCtrlLineInfo;
}
uint32_t usbserial_get_baudrate() {
    return _usbLineInfo.baud_rate;
}

// un-configure the USB endpoints
void usbserial_disable() {
    usb_disable_ep(USB_EP_CDC_NOTIFICATION);
    usb_disable_ep(USB_EP_CDC_OUT);
    usb_disable_ep(USB_EP_CDC_IN);
}

#ifdef USB_SERIAL_ECHO
uint8_t* usbserial_out_completion(uint8_t* buffer, uint8_t len, uint8_t* new_len) {
    if (len == 0) {
        // nothing received so re-use the buffer
        *new_len = 64;
        if (buffer) {
            return buffer;
        } else {
            return usbserial_buf[usbserial_active_tx_buf];
        }
    }
    // buffer is the data just received
    // send received data back to the host
    usb_ep_start_in(USB_EP_CDC_IN, buffer, len, false);

    // use the next buffer for the following receive transfer
    usbserial_active_tx_buf = (usbserial_active_tx_buf + 1) % 4;

    // specify a length and buffer to start a new transfer
    *new_len = 64;
    return usbserial_buf[usbserial_active_tx_buf];
}
#endif
