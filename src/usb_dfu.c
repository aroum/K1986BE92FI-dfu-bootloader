#include "usb_dfu.h"
#include "config.h"
#include "flash.h"
#include <string.h>

/* USB DFU State variables */
static volatile uint8_t  dfu_state = STATE_DFU_IDLE;
static uint8_t  dfu_status = DFU_STATUS_OK;
static uint32_t dfu_address = MEM_APP_START;
volatile uint8_t dfu_reset_pending = 0;

static uint8_t  dfu_rx_buffer[DFU_TRANSFER_SIZE];
static uint16_t dfu_rx_len = 0;

/* Standard Device Descriptor */
static const uint8_t Usb_Dfu_Device_Descriptor[18] = {
    0x12,                  /* bLength */
    0x01,                  /* bDescriptorType (Device) */
    0x00, 0x02,            /* bcdUSB (2.0) */
    0x00,                  /* bDeviceClass (Defined at interface level) */
    0x00,                  /* bDeviceSubClass */
    0x00,                  /* bDeviceProtocol */
    64,                    /* bMaxPacketSize0 */
    (uint8_t)(DFU_USB_VID & 0xFF),
    (uint8_t)((DFU_USB_VID >> 8) & 0xFF),     /* idVendor */
    (uint8_t)(DFU_USB_PID & 0xFF),
    (uint8_t)((DFU_USB_PID >> 8) & 0xFF),     /* idProduct */
    (uint8_t)(DFU_USB_DEVICE_RELEASE & 0xFF),
    (uint8_t)((DFU_USB_DEVICE_RELEASE >> 8) & 0xFF), /* bcdDevice */
    0x01,                  /* iManufacturer (String 1) */
    0x02,                  /* iProduct (String 2) */
    0x03,                  /* iSerialNumber (String 3) */
    0x01                   /* bNumConfigurations */
};

/* Configuration Descriptor (Config + Interface + DFU Functional) */
static const uint8_t Usb_Dfu_Config_Descriptor[27] = {
    /* Configuration Descriptor (9 bytes) */
    0x09,                  /* bLength */
    0x02,                  /* bDescriptorType (Configuration) */
    27, 0x00,              /* wTotalLength (27 bytes) */
    0x01,                  /* bNumInterfaces */
    0x01,                  /* bConfigurationValue */
    0x00,                  /* iConfiguration */
    0x80,                  /* bmAttributes (Bus Powered) */
    50,                    /* bMaxPower (100 mA) */

    /* Interface Descriptor (9 bytes) */
    0x09,                  /* bLength */
    0x04,                  /* bDescriptorType (Interface) */
    0x00,                  /* bInterfaceNumber */
    0x00,                  /* bAlternateSetting */
    0x00,                  /* bNumEndpoints (0 endpoints, uses EP0 control) */
    0xFE,                  /* bInterfaceClass (Application Specific Class) */
    0x01,                  /* bInterfaceSubClass (DFU) */
    0x02,                  /* bInterfaceProtocol (DFU mode) */
    0x04,                  /* iInterface (String 4) */

    /* DFU Functional Descriptor (9 bytes) */
    0x09,                  /* bLength */
    0x21,                  /* bDescriptorType (DFU Functional) */
    0x03,                  /* bmAttributes (Can download, Will detach) */
    0xFF, 0x00,            /* wDetachTimeout (255 ms) */
    (uint8_t)(DFU_TRANSFER_SIZE & 0xFF),
    (uint8_t)((DFU_TRANSFER_SIZE >> 8) & 0xFF), /* wTransferSize */
    0x10, 0x01             /* bcdDFUVersion (DFU 1.1) */
};

/* String Descriptor 0 (Language ID) */
static const uint8_t Usb_Dfu_String_LangID[4] = {
    0x04,                  /* bLength */
    0x03,                  /* bDescriptorType (String) */
    0x09, 0x04             /* wLANGID (US English 0x0409) */
};

/* String Descriptor 1 (Manufacturer) - "Milandr" in UTF-16LE */
static const uint8_t Usb_Dfu_String_Manuf[] = {
    16, 0x03,
    'M', 0, 'i', 0, 'l', 0, 'a', 0, 'n', 0, 'd', 0, 'r', 0
};

/* String Descriptor 2 (Product) - "Milandr DFU Bootloader" in UTF-16LE */
static const uint8_t Usb_Dfu_String_Prod[] = {
    46, 0x03,
    'M', 0, 'i', 0, 'l', 0, 'a', 0, 'n', 0, 'd', 0, 'r', 0, ' ', 0,
    'D', 0, 'F', 0, 'U', 0, ' ', 0,
    'B', 0, 'o', 0, 'o', 0, 't', 0, 'l', 0, 'o', 0, 'a', 0, 'd', 0, 'e', 0, 'r', 0
};

/* String Descriptor 3 (Serial) - "MDR32-DFU" in UTF-16LE */
static const uint8_t Usb_Dfu_String_Serial[] = {
    20, 0x03,
    'M', 0, 'D', 0, 'R', 0, '3', 0, '2', 0, '-', 0, 'D', 0, 'F', 0, 'U', 0
};

/* String Descriptor 4 (Interface) - "Milandr DFU Interface" in UTF-16LE */
static const uint8_t Usb_Dfu_String_Interface[] = {
    44, 0x03,
    'M', 0, 'i', 0, 'l', 0, 'a', 0, 'n', 0, 'd', 0, 'r', 0, ' ', 0,
    'D', 0, 'F', 0, 'U', 0, ' ', 0,
    'I', 0, 'n', 0, 't', 0, 'e', 0, 'r', 0, 'f', 0, 'a', 0, 'c', 0, 'e', 0
};

/* Helper function to program the received buffer to internal flash */
static void write_buffer_to_flash(uint32_t address, const uint8_t *buffer, uint32_t length) {
    for (uint32_t i = 0; i < length; i += 4) {
        uint32_t word = 0xFFFFFFFF;
        uint32_t bytes_to_copy = (length - i >= 4) ? 4 : (length - i);
        memcpy(&word, &buffer[i], bytes_to_copy);

        /* Erase page if the address is aligned to the page boundary */
        if ((address & (MEM_PAGE_SIZE - 1)) == 0) {
            flash_erase_page(address);
        }

        flash_write_word(address, word);
        address += 4;
    }
}

/* Callback triggered when the data stage of DFU_DNLOAD completes */
static USB_Result DFU_OnDataReceived(USB_EP_TypeDef EPx, uint8_t* Buffer, uint32_t Length) {
    (void)Buffer;

    if (Length > 0) {
        write_buffer_to_flash(dfu_address, dfu_rx_buffer, Length);
        dfu_address += Length;
    }

    dfu_state = STATE_DFU_DNLOAD_SYNC;
    return USB_EP_doDataIn(EPx, 0, 0, 0);
}

void USB_DFU_Init(void) {
    dfu_state = STATE_DFU_IDLE;
    dfu_status = DFU_STATUS_OK;
    dfu_address = MEM_APP_START;
    dfu_reset_pending = 0;
}

USB_Result USB_DFU_Reset(void) {
    if (dfu_state == STATE_DFU_MANIFEST_WAIT_RESET) {
        dfu_reset_pending = 1;
    }

    USB_Result result = USB_DeviceReset();
    if (result == USB_SUCCESS) {
        dfu_state = STATE_DFU_IDLE;
        dfu_status = DFU_STATUS_OK;
        dfu_address = MEM_APP_START;
    }
    return result;
}

USB_Result USB_DFU_SetConfiguration(uint16_t wValue) {
    (void)wValue;
    return USB_SUCCESS;
}

USB_Result USB_DFU_GetDescriptor(uint16_t wVALUE, uint16_t wINDEX, uint16_t wLENGTH) {
    const uint8_t* pDescr = 0;
    uint32_t length = 0;
    USB_Result result = USB_SUCCESS;
    uint8_t descType = (uint8_t)(wVALUE >> 8);
    uint8_t descIndex = (uint8_t)(wVALUE & 0xFF);

    (void)wINDEX;

    switch (descType) {
        case USB_DEVICE:
            pDescr = Usb_Dfu_Device_Descriptor;
            length = sizeof(Usb_Dfu_Device_Descriptor);
            break;

        case USB_CONFIGURATION:
            pDescr = Usb_Dfu_Config_Descriptor;
            length = sizeof(Usb_Dfu_Config_Descriptor);
            break;

        case USB_STRING:
            switch (descIndex) {
                case 0:
                    pDescr = Usb_Dfu_String_LangID;
                    length = sizeof(Usb_Dfu_String_LangID);
                    break;
                case 1:
                    pDescr = Usb_Dfu_String_Manuf;
                    length = sizeof(Usb_Dfu_String_Manuf);
                    break;
                case 2:
                    pDescr = Usb_Dfu_String_Prod;
                    length = sizeof(Usb_Dfu_String_Prod);
                    break;
                case 3:
                    pDescr = Usb_Dfu_String_Serial;
                    length = sizeof(Usb_Dfu_String_Serial);
                    break;
                case 4:
                    pDescr = Usb_Dfu_String_Interface;
                    length = sizeof(Usb_Dfu_String_Interface);
                    break;
                default:
                    result = USB_ERROR;
                    break;
            }
            break;

        default:
            result = USB_ERROR;
            break;
    }

    if (result == USB_SUCCESS && pDescr != 0) {
        if (length > wLENGTH) {
            length = wLENGTH;
        }
        result = USB_EP_doDataIn(USB_EP0, (uint8_t*)pDescr, length, USB_DeviceDoStatusOutAck);
    }

    return result;
}

USB_Result USB_DFU_ClassRequest(void) {
    USB_Result result = USB_SUCCESS;
    uint16_t wLength = USB_CurrentSetupPacket.wLength;
    static uint8_t status_response[6];

    switch (USB_CurrentSetupPacket.bRequest) {
        case DFU_DNLOAD:
            if (wLength > 0) {
                if (wLength > DFU_TRANSFER_SIZE) {
                    dfu_status = DFU_STATUS_ERR_ADDRESS;
                    dfu_state = STATE_DFU_ERROR;
                    result = USB_ERROR;
                } else {
                    dfu_rx_len = wLength;
                    result = USB_EP_doDataOut(USB_EP0, dfu_rx_buffer, wLength, DFU_OnDataReceived);
                }
            } else {
                /* End of download, enter manifestation sync */
                dfu_state = STATE_DFU_MANIFEST_SYNC;
                /* Send ZLP for status stage */
                result = USB_EP_doDataIn(USB_EP0, 0, 0, 0);
            }
            break;

        case DFU_GETSTATUS:
            status_response[0] = dfu_status;
            status_response[1] = 0; /* Poll Timeout LSB */
            status_response[2] = 0;
            status_response[3] = 0; /* Poll Timeout MSB */
            status_response[4] = dfu_state;
            status_response[5] = 0; /* iString */

            /* Handle State transitions when host reads status */
            if (dfu_state == STATE_DFU_DNLOAD_SYNC) {
                dfu_state = STATE_DFU_DNLOAD_IDLE;
            } else if (dfu_state == STATE_DFU_MANIFEST_SYNC) {
                dfu_state = STATE_DFU_MANIFEST_WAIT_RESET;
            }

            result = USB_EP_doDataIn(USB_EP0, status_response, 6, USB_DeviceDoStatusOutAck);
            break;

        case DFU_CLRSTATUS:
            dfu_status = DFU_STATUS_OK;
            dfu_state = STATE_DFU_IDLE;
            dfu_address = MEM_APP_START;
            /* Send status handshake */
            result = USB_EP_doDataIn(USB_EP0, 0, 0, 0);
            break;

        case DFU_GETSTATE:
            status_response[0] = dfu_state;
            result = USB_EP_doDataIn(USB_EP0, status_response, 1, USB_DeviceDoStatusOutAck);
            break;

        case DFU_ABORT:
            dfu_state = STATE_DFU_IDLE;
            dfu_status = DFU_STATUS_OK;
            dfu_address = MEM_APP_START;
            result = USB_EP_doDataIn(USB_EP0, 0, 0, 0);
            break;

        default:
            result = USB_ERROR;
            break;
    }

    return result;
}

uint8_t USB_DFU_GetState(void) {
    return dfu_state;
}
