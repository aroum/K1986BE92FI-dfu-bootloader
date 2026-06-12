#include "MDR32FxQI_bkp.h"
#include "MDR32FxQI_eeprom.h"
#include "MDR32FxQI_port.h"
#include "MDR32FxQI_rst_clk.h"
#include "MDR32FxQI_usb_device.h"
#include "config.h"
#include "flash.h"
#include "reboot.h"
#include "usb_dfu.h"
#include <stdint.h>

/* Simple software delay in microseconds */
static void delay_us(uint32_t us) {
  for (volatile uint32_t i = 0; i < us * 8; i++) {
    __NOP();
  }
}

/* System Clock Initialization to 80 MHz */
static void clock_init(void) {
  RST_CLK_DeInit();
  SystemCoreClockUpdate();

  RST_CLK_HSEconfig(RST_CLK_HSE_ON);
  while (RST_CLK_HSEstatus() == ERROR) {
  }

  /* CPU PLL clock: 8 MHz * 10 = 80 MHz */
  RST_CLK_CPU_PLLconfig(RST_CLK_CPU_PLLsrcHSEdiv1, RST_CLK_CPU_PLLmul10);
  RST_CLK_CPU_PLLcmd(ENABLE);
  while (RST_CLK_CPU_PLLstatus() == ERROR) {
  }

  RST_CLK_CPU_PLLuse(ENABLE);
  RST_CLK_CPUclkPrescaler(RST_CLK_CPUclkDIV1);

  /* EEPROM Latency 3 (for CPU frequency up to 80 MHz) */
  RST_CLK_PCLKcmd(RST_CLK_PCLK_EEPROM, ENABLE);
  EEPROM_SetLatency(EEPROM_Latency_3);
  RST_CLK_PCLKcmd(RST_CLK_PCLK_EEPROM, DISABLE);

  /* Backup domain configuration */
  RST_CLK_PCLKcmd(RST_CLK_PCLK_BKP, ENABLE);
  BKP_DUccMode(BKP_DUcc_upto_80MHz);

  RST_CLK_CPUclkSelection(RST_CLK_CPUclkCPU_C3);
  SystemCoreClockUpdate();
}

/* USB Peripheral Clock and Init */
static void usb_clock_init(void) {
  /* Enable USB Clock */
  RST_CLK_PCLKcmd(RST_CLK_PCLK_USB, ENABLE);

  USB_Clock_TypeDef USB_Clock_InitStruct;
  USB_DeviceBUSParam_TypeDef USB_DeviceBUSParam;

  /* USB C1 Source = HSE (8 MHz). PLL USBMUL = 6. 8 MHz * 6 = 48 MHz */
  USB_Clock_InitStruct.USB_USBC1_Source = USB_C1HSEdiv1;
  USB_Clock_InitStruct.USB_PLLUSBMUL = USB_PLLUSBMUL6;

  USB_DeviceBUSParam.MODE = USB_SC_SCFSP_Full;
  USB_DeviceBUSParam.SPEED = USB_SC_SCFSR_12Mb;
  USB_DeviceBUSParam.PULL = USB_HSCR_DP_PULLUP_Set;

  USB_DeviceInit(&USB_Clock_InitStruct, &USB_DeviceBUSParam);
  USB_SetSIM(USB_SIS_Msk);
  USB_DevicePowerOn();

  /* Set USB interrupt priority and enable it */
  NVIC_SetPriority(USB_IRQn, 1);
  NVIC_EnableIRQ(USB_IRQn);

  USB_DeviceReset();
}

/* Check if the user application present in Flash is valid */
static int app_is_valid(void) {
  uint32_t sp = *(volatile uint32_t *)MEM_APP_START;
  uint32_t pc = *(volatile uint32_t *)(MEM_APP_START + 4);

  /* Check if stack pointer is within RAM range */
  if ((sp & 0xFFE00000) != 0x20000000) {
    return 0;
  }
  /* Check if Reset Handler address points to Flash memory */
  if ((pc & 0xFF000000) != 0x08000000) {
    return 0;
  }
  return 1;
}

/* Jump to the user application */
static void jump_to_app(void) {
  uint32_t app_sp = *(volatile uint32_t *)MEM_APP_START;
  uint32_t app_pc = *(volatile uint32_t *)(MEM_APP_START + 4);

  __disable_irq();

  /* Disable PORT clocks to cleanup peripheral state */
  RST_CLK_PCLKcmd(RST_CLK_PCLK_PORTB, DISABLE);

  /* Set Vector Table Offset Register (VTOR) */
  *(volatile uint32_t *)(0xE000ED08) = MEM_APP_START;

  /* Set Stack Pointer and jump */
  __set_MSP(app_sp);
  void (*app_reset)(void) = (void (*)(void))app_pc;
  app_reset();
}

int main(void) {
  /* Enable PORTB Clock for Button (PB6) and LED (PB7) */
  RST_CLK_PCLKcmd(DFU_BTN_PCLK, ENABLE);

  /* Configure DFU Button pin (PB6) as Input */
  PORT_InitTypeDef PORT_InitStructure;
  PORT_StructInit(&PORT_InitStructure);
  PORT_InitStructure.PORT_Pin = DFU_BTN_PIN;
  PORT_InitStructure.PORT_OE = PORT_OE_IN;
  PORT_InitStructure.PORT_FUNC = PORT_FUNC_PORT;
  PORT_InitStructure.PORT_MODE = PORT_MODE_DIGITAL;
  PORT_InitStructure.PORT_SPEED = PORT_SPEED_SLOW;
#if DFU_BTN_ACTIVE_LOW
  PORT_InitStructure.PORT_PULL_UP = PORT_PULL_UP_ON;
#else
  PORT_InitStructure.PORT_PULL_DOWN = PORT_PULL_DOWN_ON;
#endif
  PORT_Init(DFU_BTN_PORT, &PORT_InitStructure);

  /* Small stabilization delay */
  delay_us(10000);

  /* Read the button state */
  uint8_t btn_pressed = 0;
#if DFU_BTN_ACTIVE_LOW
  btn_pressed = (PORT_ReadInputDataBit(DFU_BTN_PORT, DFU_BTN_PIN) == 0);
#else
  btn_pressed = (PORT_ReadInputDataBit(DFU_BTN_PORT, DFU_BTN_PIN) != 0);
#endif

  /* Check DFU entry conditions */
  int enter_dfu = 0;
  if (btn_pressed) {
    enter_dfu = 1;
  } else if (rebooted_into_dfu()) {
    enter_dfu = 1;
  } else if (!app_is_valid()) {
    enter_dfu = 1;
  }

  if (!enter_dfu) {
    /* Jump to user application directly */
    jump_to_app();
  }

  /* Configure DFU Status LED pin (PB7) as Output */
  PORT_StructInit(&PORT_InitStructure);
  PORT_InitStructure.PORT_Pin = DFU_LED_PIN;
  PORT_InitStructure.PORT_OE = PORT_OE_OUT;
  PORT_InitStructure.PORT_FUNC = PORT_FUNC_PORT;
  PORT_InitStructure.PORT_MODE = PORT_MODE_DIGITAL;
  PORT_InitStructure.PORT_SPEED = PORT_SPEED_SLOW;
  PORT_Init(DFU_LED_PORT, &PORT_InitStructure);

  /* Turn on/off LED initially */
#if DFU_LED_ACTIVE_HIGH
  PORT_SetBits(DFU_LED_PORT, DFU_LED_PIN);
#else
  PORT_ResetBits(DFU_LED_PORT, DFU_LED_PIN);
#endif

  /* Initialize System and USB Clock */
  clock_init();

  /* Re-enable PORT clock for LED after clock_init() resets peripheral clocks */
  RST_CLK_PCLKcmd(DFU_BTN_PCLK, ENABLE);

  /* Re-initialize LED pin configuration */
  PORT_StructInit(&PORT_InitStructure);
  PORT_InitStructure.PORT_Pin = DFU_LED_PIN;
  PORT_InitStructure.PORT_OE = PORT_OE_OUT;
  PORT_InitStructure.PORT_FUNC = PORT_FUNC_PORT;
  PORT_InitStructure.PORT_MODE = PORT_MODE_DIGITAL;
  PORT_InitStructure.PORT_SPEED = PORT_SPEED_SLOW;
  PORT_Init(DFU_LED_PORT, &PORT_InitStructure);

  flash_init();
  USB_DFU_Init();
  usb_clock_init();

  /* Enable Interrupts globally for USB handling */
  __enable_irq();

  uint32_t blink_counter = 0;
  uint8_t led_state = 1;

  while (1) {
    if (dfu_reset_pending) {
      delay_us(200000); /* Wait 200 ms for USB reset handshake to complete on
                           host side */
      NVIC_SystemReset();
    }

    delay_us(1000);
    blink_counter++;
    if (blink_counter >= 150) {
      blink_counter = 0;
      led_state = !led_state;

      if (led_state) {
#if DFU_LED_ACTIVE_HIGH
        PORT_SetBits(DFU_LED_PORT, DFU_LED_PIN);
#else
        PORT_ResetBits(DFU_LED_PORT, DFU_LED_PIN);
#endif
      } else {
#if DFU_LED_ACTIVE_HIGH
        PORT_ResetBits(DFU_LED_PORT, DFU_LED_PIN);
#else
        PORT_SetBits(DFU_LED_PORT, DFU_LED_PIN);
#endif
      }
    }
  }
}
