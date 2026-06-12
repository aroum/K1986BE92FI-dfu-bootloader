#ifndef __MDR32FxQI_USB_HANDLERS_H
#define __MDR32FxQI_USB_HANDLERS_H

#include "MDR32FxQI_usb_default_handlers.h"

/* Override handlers back to the standard library dummy handlers */
#undef USB_DEVICE_HANDLE_RESET
#define USB_DEVICE_HANDLE_RESET  USB_DeviceReset()

#undef USB_DEVICE_HANDLE_GET_DESCRIPTOR
#define USB_DEVICE_HANDLE_GET_DESCRIPTOR(wVALUE, wINDEX, wLENGTH)  USB_DeviceDummyGetDescriptor(wVALUE, wINDEX, wLENGTH)

#undef USB_DEVICE_HANDLE_CLASS_REQUEST
#define USB_DEVICE_HANDLE_CLASS_REQUEST  USB_DeviceDummyClassRequest()

#undef USB_DEVICE_HANDLE_SET_CONFIGURATION
#define USB_DEVICE_HANDLE_SET_CONFIGURATION(wVALUE)  USB_DeviceDummySetConfiguration(wVALUE)

#endif /* __MDR32FxQI_USB_HANDLERS_H */
