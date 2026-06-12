#ifndef __USB_DFU_H
#define __USB_DFU_H

#include <stdint.h>
#include "MDR32FxQI_usb_device.h"

/* DFU Class Requests */
#define DFU_DETACH          0
#define DFU_DNLOAD          1
#define DFU_UPLOAD          2
#define DFU_GETSTATUS       3
#define DFU_CLRSTATUS       4
#define DFU_GETSTATE        5
#define DFU_ABORT           6

/* DFU Status Codes */
#define DFU_STATUS_OK               0
#define DFU_STATUS_ERR_TARGET       1
#define DFU_STATUS_ERR_FILE         2
#define DFU_STATUS_ERR_WRITE        3
#define DFU_STATUS_ERR_ERASE        4
#define DFU_STATUS_ERR_CHECK_ERASED 5
#define DFU_STATUS_ERR_PROG         6
#define DFU_STATUS_ERR_VERIFY       7
#define DFU_STATUS_ERR_ADDRESS      8
#define DFU_STATUS_ERR_NOTDONE       9
#define DFU_STATUS_ERR_FIRMWARE     10
#define DFU_STATUS_ERR_VENDOR       11
#define DFU_STATUS_ERR_USBR         12
#define DFU_STATUS_ERR_POR          13
#define DFU_STATUS_ERR_UNKNOWN      14
#define DFU_STATUS_ERR_STALLEDPKT   15

/* DFU State Codes */
#define STATE_DFU_IDLE                  2
#define STATE_DFU_DNLOAD_SYNC           3
#define STATE_DFU_DNBUSY                4
#define STATE_DFU_DNLOAD_IDLE           5
#define STATE_DFU_MANIFEST_SYNC         6
#define STATE_DFU_MANIFEST              7
#define STATE_DFU_MANIFEST_WAIT_RESET   8
#define STATE_DFU_UPLOAD_IDLE           9
#define STATE_DFU_ERROR                 10

#define DFU_TRANSFER_SIZE   1024

/* DFU Interface Functions */
void USB_DFU_Init(void);
USB_Result USB_DFU_Reset(void);
USB_Result USB_DFU_GetDescriptor(uint16_t wVALUE, uint16_t wINDEX, uint16_t wLENGTH);
USB_Result USB_DFU_ClassRequest(void);
USB_Result USB_DFU_SetConfiguration(uint16_t wValue);
uint8_t USB_DFU_GetState(void);

extern volatile uint8_t dfu_reset_pending;

#endif /* __USB_DFU_H */
