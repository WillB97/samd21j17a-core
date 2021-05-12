#include "USB-CDC.h"

USB_ENDPOINTS(3);

// USB serial function prototypes
void usbserial_init();
void usbserial_in_completion();
void usbserial_out_completion();

USB_ALIGN const USB_DeviceDescriptor device_descriptor = {
    .bLength = sizeof(USB_DeviceDescriptor),
    .bDescriptorType = USB_DTYPE_Device,

    .bcdUSB                 = 0x0200,  // USB version 2.0
    .bDeviceClass           = 0,
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
        .bmCapabilities = 0x06,  // supports setting line coding, arduino seems to use 6
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
                    address = samd_serial_number_string_descriptor();
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
        usbserial_out_completion();
        usb_ep_handled(USB_EP_CDC_OUT);
    }

    if (usb_ep_pending(USB_EP_CDC_IN)) {
        // if a device to host serial transmission was pending,
        // run the callback and mark it as completed
        usbserial_in_completion();
        usb_ep_handled(USB_EP_CDC_IN);
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
/// TODO: Add callback configuration to link this to a USBSerial C++ class

void usbserial_init() { // called by the SET_CONFIGURATION callback
    // configure the USB endpoints
    usb_enable_ep(USB_EP_CDC_NOTIFICATION, USB_EP_TYPE_INTERRUPT, 8);
    usb_enable_ep(USB_EP_CDC_OUT, USB_EP_TYPE_BULK, 64);
    usb_enable_ep(USB_EP_CDC_IN, USB_EP_TYPE_BULK, 64);
}

// Callback, called when a USB host to device transfer completes
void usbserial_out_completion() {
    // uint32_t len = usb_ep_out_length(USB_EP_CDC_OUT);
}

// Callback, called when a USB device to host transfer completes
void usbserial_in_completion() {
}

// un-configure the USB endpoints
void usbserial_disable() {
    usb_disable_ep(USB_EP_CDC_NOTIFICATION);
    usb_disable_ep(USB_EP_CDC_OUT);
    usb_disable_ep(USB_EP_CDC_IN);
}
