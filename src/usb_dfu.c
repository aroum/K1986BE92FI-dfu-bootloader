#include "usb_dfu.h"
#include "config.h"
#include "flash.h"

/* USB DFU State variables */
static volatile uint8_t  dfu_state = STATE_DFU_IDLE;
static uint8_t  dfu_status = DFU_STATUS_OK;
static uint32_t dfu_address = MEM_APP_START;
static uint8_t  dfu_alt = 0;
volatile uint8_t dfu_reset_pending = 0;

static uint8_t  dfu_rx_buffer[DFU_TRANSFER_SIZE];
static uint16_t dfu_rx_len = 0;

#define USB_BOS_DESCRIPTOR_TYPE                 0x0F
#define USB_DEVICE_CAPABILITY_TYPE              0x10
#define DESCR_SIZE_BOS_DESC                     33
#define DESCR_SIZE_MS_OS_20_DESC                162
#define USB_BOS                                 0x0F
#define MS_OS_20_VENDOR_CODE                    0x02

/* Standard Device Descriptor */
static const uint8_t Usb_Dfu_Device_Descriptor[18] = {
    0x12,                  /* bLength */
    0x01,                  /* bDescriptorType (Device) */
    0x10, 0x02,            /* bcdUSB (2.10 for BOS and MS OS 2.0 support) */
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

/* BOS Descriptor */
static const uint8_t Usb_Dfu_Bos_Descriptor[DESCR_SIZE_BOS_DESC] = {
    /* BOS Header (5 bytes) */
    0x05,                               /* bLength */
    USB_BOS_DESCRIPTOR_TYPE,            /* bDescriptorType */
    (uint8_t)(DESCR_SIZE_BOS_DESC & 0xFF),
    (uint8_t)((DESCR_SIZE_BOS_DESC >> 8) & 0xFF), /* wTotalLength */
    0x01,                               /* bNumDeviceCaps */

    /* Device Capability: Platform (28 bytes) */
    0x1C,                               /* bLength */
    USB_DEVICE_CAPABILITY_TYPE,         /* bDescriptorType */
    0x05,                               /* bDevCapabilityType = PLATFORM */
    0x00,                               /* bReserved */

    /* MS OS 2.0 Platform Capability ID: {D8DD60DF-4589-4CC7-9CD2-659D9E648A9F} */
    0xDF, 0x60, 0xDD, 0xD8, 0x89, 0x45, 0xC7, 0x4C,
    0x9C, 0xD2, 0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F,

    /* dwWindowsVersion: Windows 8.1+ / 10 / 11 (0x06030000) */
    0x00, 0x00, 0x03, 0x06,

    /* wMSOSDescriptorSetTotalLength */
    (uint8_t)(DESCR_SIZE_MS_OS_20_DESC & 0xFF),
    (uint8_t)((DESCR_SIZE_MS_OS_20_DESC >> 8) & 0xFF),

    MS_OS_20_VENDOR_CODE,               /* bMS_VendorCode */
    0x00                                /* bAltEnumCode */
};

/* Microsoft OS 2.0 Descriptor Set (Auto WinUSB bind) */
static const uint8_t Usb_Dfu_Ms_Os_20_Descriptor[DESCR_SIZE_MS_OS_20_DESC] = {
    /* 1. MS OS 2.0 Descriptor Set Header (10 bytes) */
    0x0A, 0x00,                         /* wLength: 10 bytes */
    0x00, 0x00,                         /* wDescriptorType: Set Header */
    0x00, 0x00, 0x03, 0x06,             /* dwWindowsVersion: 0x06030000 */
    (uint8_t)(DESCR_SIZE_MS_OS_20_DESC & 0xFF),
    (uint8_t)((DESCR_SIZE_MS_OS_20_DESC >> 8) & 0xFF), /* wTotalLength */

    /* 2. Compatible ID Descriptor (20 bytes) */
    0x14, 0x00,                         /* wLength: 20 bytes */
    0x03, 0x00,                         /* wDescriptorType: Compatible ID */
    'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,       /* CompatibleID: "WINUSB" */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* SubCompatibleID */

    /* 3. Registry Property Descriptor (132 bytes) */
    0x84, 0x00,                         /* wLength: 132 bytes */
    0x04, 0x00,                         /* wDescriptorType: Registry Property */
    0x07, 0x00,                         /* wPropertyDataType: REG_MULTI_SZ */
    0x2A, 0x00,                         /* wPropertyNameLength: 42 bytes */

    /* "DeviceInterfaceGUIDs\0" in UTF-16LE */
    'D',0,'e',0,'v',0,'i',0,'c',0,'e',0,'I',0,'n',0,'t',0,'e',0,'r',0,'f',0,'a',0,'c',0,'e',0,'G',0,'U',0,'I',0,'D',0,'s',0, 0,0,

    0x50, 0x00,                         /* wPropertyDataLength: 80 bytes */

    /* "{fe1a73a0-4764-471a-a2f1-98e321c97a75}\0\0" in UTF-16LE */
    '{',0,'f',0,'e',0,'1',0,'a',0,'7',0,'3',0,'a',0,'0',0,'-',0,
    '4',0,'7',0,'6',0,'4',0,'-',0,'4',0,'7',0,'1',0,'a',0,'-',0,
    'a',0,'2',0,'f',0,'1',0,'-',0,'9',0,'8',0,'e',0,'3',0,'2',0,
    '1',0,'c',0,'9',0,'7',0,'a',0,'7',0,'5',0,'}',0,
    0,0, 0,0
};

/* Configuration Descriptor (Config + 2 Interfaces + DFU Functional = 36 bytes) */
static const uint8_t Usb_Dfu_Config_Descriptor[36] = {
    /* Configuration Descriptor (9 bytes) */
    0x09,                  /* bLength */
    0x02,                  /* bDescriptorType (Configuration) */
    36, 0x00,              /* wTotalLength (36 bytes) */
    0x01,                  /* bNumInterfaces */
    0x01,                  /* bConfigurationValue */
    0x00,                  /* iConfiguration */
    0x80,                  /* bmAttributes (Bus Powered) */
    50,                    /* bMaxPower (100 mA) */

    /* Interface Descriptor Alt 0: Internal FLASH (9 bytes) */
    0x09,                  /* bLength */
    0x04,                  /* bDescriptorType (Interface) */
    0x00,                  /* bInterfaceNumber */
    0x00,                  /* bAlternateSetting */
    0x00,                  /* bNumEndpoints (0 endpoints, uses EP0 control) */
    0xFE,                  /* bInterfaceClass (Application Specific Class) */
    0x01,                  /* bInterfaceSubClass (DFU) */
    0x02,                  /* bInterfaceProtocol (DFU mode) */
    0x04,                  /* iInterface (String 4: Internal FLASH) */

    /* Interface Descriptor Alt 1: Internal EEPROM (9 bytes) */
    0x09,                  /* bLength */
    0x04,                  /* bDescriptorType (Interface) */
    0x00,                  /* bInterfaceNumber */
    0x01,                  /* bAlternateSetting */
    0x00,                  /* bNumEndpoints */
    0xFE,                  /* bInterfaceClass (Application Specific Class) */
    0x01,                  /* bInterfaceSubClass (DFU) */
    0x02,                  /* bInterfaceProtocol (DFU mode) */
    0x05,                  /* iInterface (String 5: Internal EEPROM) */

    /* DFU Functional Descriptor (9 bytes) */
    0x09,                  /* bLength */
    0x21,                  /* bDescriptorType (DFU Functional) */
    0x0B,                  /* bmAttributes (Can download, Can upload, Will detach) */
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

/* String Descriptor 2 (Product) - "MDR32 DFU" in UTF-16LE */
static const uint8_t Usb_Dfu_String_Prod[] = {
    20, 0x03,
    'M', 0, 'D', 0, 'R', 0, '3', 0, '2', 0, ' ', 0,
    'D', 0, 'F', 0, 'U', 0
};

/* String Descriptor 3 (Serial) - "MDR32-DFU" in UTF-16LE */
static const uint8_t Usb_Dfu_String_Serial[] = {
    20, 0x03,
    'M', 0, 'D', 0, 'R', 0, '3', 0, '2', 0, '-', 0, 'D', 0, 'F', 0, 'U', 0
};

/* String Descriptor 4 (Interface 0) - "Internal FLASH" in UTF-16LE */
static const uint8_t Usb_Dfu_String_IntfFlash[] = {
    30, 0x03,
    'I', 0, 'n', 0, 't', 0, 'e', 0, 'r', 0, 'n', 0, 'a', 0, 'l', 0, ' ', 0,
    'F', 0, 'L', 0, 'A', 0, 'S', 0, 'H', 0
};

/* String Descriptor 5 (Interface 1) - "Internal EEPROM" in UTF-16LE */
static const uint8_t Usb_Dfu_String_IntfEeprom[] = {
    32, 0x03,
    'I', 0, 'n', 0, 't', 0, 'e', 0, 'r', 0, 'n', 0, 'a', 0, 'l', 0, ' ', 0,
    'E', 0, 'E', 0, 'P', 0, 'R', 0, 'O', 0, 'M', 0
};

static const uint8_t * const Usb_Dfu_Strings[] = {
    Usb_Dfu_String_LangID,
    Usb_Dfu_String_Manuf,
    Usb_Dfu_String_Prod,
    Usb_Dfu_String_Serial,
    Usb_Dfu_String_IntfFlash,
    Usb_Dfu_String_IntfEeprom,
};

static uint32_t get_mem_base(void) {
    return (dfu_alt == 0) ? MEM_APP_START : MEM_FLASH_START;
}

static uint32_t get_mem_limit(void) {
    return (dfu_alt == 0) ? (MEM_FLASH_START + MEM_FLASH_SIZE) : (MEM_FLASH_START + MEM_INFO_SIZE);
}

/* Helper function to program the received buffer to internal flash */
static void write_buffer_to_flash(uint32_t address, const uint8_t *buffer, uint32_t length) {
    EEPROM_Mem_Bank bank = (dfu_alt == 0) ? EEPROM_Main_Bank_Select : EEPROM_Info_Bank_Select;
    for (uint32_t i = 0; i < length; i += 4) {
        uint32_t word = 0xFFFFFFFF;
        uint32_t bytes_to_copy = (length - i >= 4) ? 4 : (length - i);
        for (uint32_t b = 0; b < bytes_to_copy; b++) {
            ((uint8_t *)&word)[b] = buffer[i + b];
        }

        /* Erase page if the address is aligned to the page boundary */
        if ((address & (MEM_PAGE_SIZE - 1)) == 0) {
            flash_erase_page(address, bank);
        }

        flash_write_word(address, word, bank);
        address += 4;
    }
}

/* Callback triggered when the data stage of DFU_DNLOAD completes */
static USB_Result DFU_OnDataReceived(USB_EP_TypeDef EPx, uint8_t* Buffer, uint32_t Length) {
    (void)Buffer;

    if (Length > 0) {
        if (dfu_address < get_mem_base() || (dfu_address + Length) > get_mem_limit()) {
            dfu_status = DFU_STATUS_ERR_ADDRESS;
            dfu_state = STATE_DFU_ERROR;
            return USB_ERROR;
        }
        write_buffer_to_flash(dfu_address, dfu_rx_buffer, Length);
        dfu_address += Length;
    }

    dfu_state = STATE_DFU_DNLOAD_SYNC;
    return USB_EP_doDataIn(EPx, 0, 0, 0);
}

void USB_DFU_Init(void) {
    dfu_state = STATE_DFU_IDLE;
    dfu_status = DFU_STATUS_OK;
    dfu_address = get_mem_base();
    dfu_reset_pending = 0;
}

USB_Result USB_DFU_Reset(void) {
    if (dfu_state == STATE_DFU_MANIFEST_WAIT_RESET || dfu_state == STATE_DFU_MANIFEST_SYNC) {
        dfu_reset_pending = 1;
    }

    USB_Result result = USB_DeviceReset();
    if (result == USB_SUCCESS) {
        dfu_state = STATE_DFU_IDLE;
        dfu_status = DFU_STATUS_OK;
        dfu_address = get_mem_base();
    }
    return result;
}

USB_Result USB_DFU_SetConfiguration(uint16_t wValue) {
    (void)wValue;
    return USB_SUCCESS;
}

USB_Result USB_DFU_SetInterface(uint16_t wValue, uint16_t wIndex) {
    if (wIndex == 0 && wValue <= 1) {
        dfu_alt = (uint8_t)wValue;
        dfu_address = get_mem_base();
        return USB_SUCCESS;
    }
    return USB_ERROR;
}

uint8_t USB_DFU_GetInterface(uint16_t wIndex) {
    (void)wIndex;
    return dfu_alt;
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

        case USB_BOS:
            pDescr = Usb_Dfu_Bos_Descriptor;
            length = sizeof(Usb_Dfu_Bos_Descriptor);
            break;

        case USB_STRING:
            if (descIndex < sizeof(Usb_Dfu_Strings) / sizeof(Usb_Dfu_Strings[0])) {
                pDescr = Usb_Dfu_Strings[descIndex];
                length = pDescr[0];
            } else {
                result = USB_ERROR;
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

static USB_Result on_set_address(USB_EP_TypeDef EPx, uint8_t* Buffer, uint32_t Length) {
    (void)EPx; (void)Buffer; (void)Length;
    USB_SetSA(USB_DeviceContext.Address);
    USB_DeviceContext.USB_DeviceState = USB_DeviceContext.Address ? USB_DEV_STATE_ADDRESS : USB_DEV_STATE_DEFAULT;
    return USB_SUCCESS;
}

USB_Result USB_DFU_SetupPacket(USB_EP_TypeDef EPx, const USB_SetupPacket_TypeDef* p) {
    uint8_t reqType = p->mRequestTypeData & 0x60;
    if (reqType == 0x00) { /* Standard USB request */
        switch (p->bRequest) {
            case USB_GET_DESCRIPTOR:
                return USB_DFU_GetDescriptor(p->wValue, p->wIndex, p->wLength);
            case USB_SET_ADDRESS:
                USB_DeviceContext.Address = p->wValue;
                return USB_EP_doDataIn(EPx, 0, 0, on_set_address);
            case USB_SET_CONFIGURATION:
                USB_DeviceContext.USB_DeviceState = USB_DEV_STATE_CONFIGURED;
                return USB_EP_doDataIn(EPx, 0, 0, 0);
            case USB_GET_CONFIGURATION: {
                static uint8_t cfg = 1;
                return USB_EP_doDataIn(EPx, &cfg, 1, USB_DeviceDoStatusOutAck);
            }
            case USB_SET_INTERFACE:
                if (USB_DFU_SetInterface(p->wValue, p->wIndex) == USB_SUCCESS) {
                    return USB_EP_doDataIn(EPx, 0, 0, 0);
                }
                return USB_EP_Stall(EPx, USB_STALL_PROTO);
            case USB_GET_INTERFACE: {
                static uint8_t alt;
                alt = USB_DFU_GetInterface(p->wIndex);
                return USB_EP_doDataIn(EPx, &alt, 1, USB_DeviceDoStatusOutAck);
            }
            case USB_GET_STATUS: {
                static uint16_t status = 0;
                return USB_EP_doDataIn(EPx, (uint8_t *)&status, 2, USB_DeviceDoStatusOutAck);
            }
            default:
                return USB_EP_doDataIn(EPx, 0, 0, 0);
        }
    } else if (reqType == 0x20) { /* Class request (DFU) */
        return USB_DFU_ClassRequest();
    } else if (reqType == 0x40) { /* Vendor request (MS OS 2.0) */
        return USB_DFU_VendorRequest();
    }
    return USB_EP_Stall(EPx, USB_STALL_PROTO);
}

USB_Result USB_DFU_VendorRequest(void) {
    USB_RequestTypeDT_TypeDef dir = (USB_RequestTypeDT_TypeDef)(USB_CurrentSetupPacket.mRequestTypeData & USB_REQUEST_DT_Msk);
    if (dir == USB_DEVICE_TO_HOST) {
        if (USB_CurrentSetupPacket.bRequest == MS_OS_20_VENDOR_CODE &&
            USB_CurrentSetupPacket.wIndex == 0x0007) {
            uint32_t len = sizeof(Usb_Dfu_Ms_Os_20_Descriptor);
            if (len > USB_CurrentSetupPacket.wLength) {
                len = USB_CurrentSetupPacket.wLength;
            }
            return USB_EP_doDataIn(USB_EP0, (uint8_t *)Usb_Dfu_Ms_Os_20_Descriptor, len, USB_DeviceDoStatusOutAck);
        }
    }
    return USB_ERROR;
}

USB_Result USB_DFU_ClassRequest(void) {
    USB_Result result = USB_SUCCESS;
    uint16_t wLength = USB_CurrentSetupPacket.wLength;
    static uint8_t status_response[6];

    switch (USB_CurrentSetupPacket.bRequest) {
        case DFU_DETACH:
            dfu_reset_pending = 1;
            /* Send ZLP acknowledge */
            result = USB_EP_doDataIn(USB_EP0, 0, 0, 0);
            break;

        case DFU_DNLOAD:
            if (wLength > 0) {
                if (wLength > DFU_TRANSFER_SIZE ||
                    dfu_address < get_mem_base() ||
                    (dfu_address + wLength) > get_mem_limit()) {
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

        case DFU_UPLOAD:
            if (dfu_state == STATE_DFU_IDLE) {
                dfu_address = get_mem_base();
            }
            if (dfu_address >= get_mem_limit()) {
                /* Finished: send ZLP to terminate upload */
                dfu_state = STATE_DFU_IDLE;
                result = USB_EP_doDataIn(USB_EP0, 0, 0, USB_DeviceDoStatusOutAck);
            } else {
                uint32_t chunk = get_mem_limit() - dfu_address;
                if (chunk > wLength) {
                    chunk = wLength;
                }
                EEPROM_Mem_Bank bank = (dfu_alt == 0) ? EEPROM_Main_Bank_Select : EEPROM_Info_Bank_Select;
                flash_read_block(dfu_address, dfu_rx_buffer, chunk, bank);
                result = USB_EP_doDataIn(USB_EP0, dfu_rx_buffer, chunk, USB_DeviceDoStatusOutAck);
                dfu_address += chunk;
                dfu_state = (chunk < wLength) ? STATE_DFU_IDLE : STATE_DFU_UPLOAD_IDLE;
            }
            break;

        case DFU_GETSTATUS:
            /* Handle State transitions when host reads status */
            if (dfu_state == STATE_DFU_DNLOAD_SYNC) {
                dfu_state = STATE_DFU_DNLOAD_IDLE;
            } else if (dfu_state == STATE_DFU_MANIFEST_SYNC) {
                dfu_state = STATE_DFU_MANIFEST_WAIT_RESET;
                dfu_reset_pending = 1;
            }

            status_response[0] = dfu_status;
            status_response[1] = 0; /* Poll Timeout LSB */
            status_response[2] = 0;
            status_response[3] = 0; /* Poll Timeout MSB */
            status_response[4] = dfu_state;
            status_response[5] = 0; /* iString */

            result = USB_EP_doDataIn(USB_EP0, status_response, 6, USB_DeviceDoStatusOutAck);
            break;

        case DFU_CLRSTATUS:
            dfu_status = DFU_STATUS_OK;
            dfu_state = STATE_DFU_IDLE;
            dfu_address = get_mem_base();
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
            dfu_address = get_mem_base();
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
