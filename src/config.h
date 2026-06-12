#ifndef __CONFIG_H
#define __CONFIG_H

#include "MDR32FxQI_port.h"
#include "MDR32FxQI_rst_clk.h"

/* Memory configurations */
#define MEM_FLASH_START          0x08000000
#define MEM_BOOTLOADER_SIZE      0x00002000  /* 8 KB reserved for bootloader */
#define MEM_APP_START            (MEM_FLASH_START + MEM_BOOTLOADER_SIZE)
#define MEM_PAGE_SIZE            0x00001000  /* 4 KB page size for MDR32F9Q2I */
#define MEM_FLASH_SIZE           0x00020000  /* 128 KB total flash */

/* GPIO Configuration for DFU activation button (PB6) */
#define DFU_BTN_PORT             MDR_PORTB
#define DFU_BTN_PIN              PORT_Pin_6
#define DFU_BTN_PCLK             RST_CLK_PCLK_PORTB
#define DFU_BTN_ACTIVE_LOW       1           /* 1 = button active low (pull-up), 0 = active high (pull-down) */

/* GPIO Configuration for DFU status LED (PB7) */
#define DFU_LED_PORT             MDR_PORTB
#define DFU_LED_PIN              PORT_Pin_7
#define DFU_LED_PCLK             RST_CLK_PCLK_PORTB
#define DFU_LED_ACTIVE_HIGH      1           /* 1 = high turns on LED, 0 = low turns on LED */

/* USB Configuration */
#define DFU_USB_VID              0x1209      /* Generic Open-Source VID */
#define DFU_USB_PID              0xBE92      /* Custom PID for Milandr DFU Bootloader */
#define DFU_USB_DEVICE_RELEASE   0x0100      /* v1.00 */

/* Magic key in RAM to stay in bootloader */
#define REBOOT_DFU_MAGIC         0xDEADBEEFCC00FFEEULL

#endif /* __CONFIG_H */
