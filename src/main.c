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

/* Stub out static C++ constructors to save flash space */
void __libc_init_array(void) {
}

/* Simple software delay in microseconds */
static void delay_us(uint32_t us) {
  for (volatile uint32_t i = 0; i < us * 8; i++) {
    __NOP();
  }
}

/* System Clock Initialization to 80 MHz */
static void clock_init(void) {
  /* Turn on HSE oscillator (8 MHz external crystal) */
  MDR_RST_CLK->HS_CONTROL |= RST_CLK_HSE_ON;
  while ((MDR_RST_CLK->CLOCK_STATUS & RST_CLK_CLOCK_STATUS_HSE_RDY) == 0) {
  }

  /* CPU PLL clock: 8 MHz * 10 = 80 MHz */
  /* Select HSE as CPU_PLL source (CPU_C1 = HSE) */
  MDR_RST_CLK->CPU_CLOCK = (MDR_RST_CLK->CPU_CLOCK & ~3) | 0x02;
  /* Set CPU PLL multiplier: 10 (multiplier value in register is mul - 1 = 9) */
  MDR_RST_CLK->PLL_CONTROL = (MDR_RST_CLK->PLL_CONTROL & ~0x0F00) | (9 << 8);
  /* Enable CPU PLL */
  MDR_RST_CLK->PLL_CONTROL |= RST_CLK_PLL_CONTROL_PLL_CPU_ON;
  while ((MDR_RST_CLK->CLOCK_STATUS & RST_CLK_CLOCK_STATUS_PLL_CPU_RDY) == 0) {
  }

  /* Enable CPU PLL output (CPU_C2 = PLL) and set prescaler to div1 (CPU_C3 = CPU_C2) */
  MDR_RST_CLK->CPU_CLOCK |= (1 << 2);

  /* EEPROM Latency 3 (for CPU frequency up to 80 MHz) */
  MDR_RST_CLK->PER_CLOCK |= RST_CLK_PCLK_EEPROM;
  MDR_EEPROM->CMD = EEPROM_Latency_3;

  /* Backup domain configuration (DUcc mode up to 80 MHz) */
  MDR_RST_CLK->PER_CLOCK |= RST_CLK_PCLK_BKP;
  MDR_BKP->REG_0E = (MDR_BKP->REG_0E & ~(BKP_REG_0E_SELECTRI_Msk | BKP_REG_0E_LOW_Msk)) |
                    (6 | (6 << BKP_REG_0E_SELECTRI_Pos));

  /* Switch CPU clock to CPU_C3 */
  MDR_RST_CLK->CPU_CLOCK = (MDR_RST_CLK->CPU_CLOCK & ~0x0300) | (1 << 8);
}

/* USB Peripheral Clock and Init */
static void usb_clock_init(void) {
  /* Enable USB Clock */
  MDR_RST_CLK->PER_CLOCK |= RST_CLK_PCLK_USB;

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

  /* Disable PORTB clock to cleanup peripheral state */
  MDR_RST_CLK->PER_CLOCK &= ~RST_CLK_PCLK_PORTB;

  /* Set Vector Table Offset Register (VTOR) */
  *(volatile uint32_t *)(0xE000ED08) = MEM_APP_START;

  /* Set Stack Pointer and jump */
  __set_MSP(app_sp);
  void (*app_reset)(void) = (void (*)(void))app_pc;
  app_reset();
}

int main(void) {
  /* Enable PORTB Clock for Button (PB6) and LED (PB7) */
  MDR_RST_CLK->PER_CLOCK |= DFU_BTN_PCLK;

  /* Configure DFU Button pin (PB6) as Digital Input */
  DFU_BTN_PORT->ANALOG |= DFU_BTN_PIN;
  DFU_BTN_PORT->OE &= ~DFU_BTN_PIN;
#if DFU_BTN_ACTIVE_LOW
  DFU_BTN_PORT->PULL |= (DFU_BTN_PIN << 16); /* Pull-up */
#else
  DFU_BTN_PORT->PULL |= DFU_BTN_PIN;         /* Pull-down */
#endif

  /* Small stabilization delay */
  delay_us(10000);

  /* Read the button state */
#if DFU_BTN_ACTIVE_LOW
  uint8_t btn_pressed = ((DFU_BTN_PORT->RXTX & DFU_BTN_PIN) == 0);
#else
  uint8_t btn_pressed = ((DFU_BTN_PORT->RXTX & DFU_BTN_PIN) != 0);
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

  /* Configure DFU Status LED pin (PB7) as Digital Output */
  DFU_LED_PORT->ANALOG |= DFU_LED_PIN;
  DFU_LED_PORT->OE |= DFU_LED_PIN;

  /* Turn on/off LED initially */
#if DFU_LED_ACTIVE_HIGH
  DFU_LED_PORT->RXTX |= DFU_LED_PIN;
#else
  DFU_LED_PORT->RXTX &= ~DFU_LED_PIN;
#endif

  /* Initialize System and USB Clock */
  clock_init();

  flash_init();
  USB_DFU_Init();
  usb_clock_init();

  /* Enable Interrupts globally for USB handling */
  __enable_irq();

  uint32_t blink_counter = 0;

  while (1) {
    if (dfu_reset_pending) {
      delay_us(200000); /* Wait 200 ms for USB reset handshake to complete on host side */
      NVIC_SystemReset();
    }

    delay_us(1000);
    blink_counter++;
    if (blink_counter >= 150) {
      blink_counter = 0;
      DFU_LED_PORT->RXTX ^= DFU_LED_PIN;
    }
  }
}
