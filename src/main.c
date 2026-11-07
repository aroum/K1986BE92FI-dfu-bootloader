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

/* Startup runs before RAM functions are copied; calibration is done in main. */
void SystemInit(void) { }
void __libc_init_array(void) { }

/* Simple software delay in microseconds */
static void button_settle(void) {
  uint32_t count = 400;
  __asm volatile ("1: subs %0, #1\n bne 1b" : "+r" (count) : : "cc");
}

/* Poll the core-clocked SysTick. Interrupts remain available to USB. */
static void delay_ms(uint32_t ms) {
  SysTick->LOAD = 80000 - 1;
  SysTick->VAL = 0;
  SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
  while (ms--) {
    while (!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)) {
    }
  }
  SysTick->CTRL = 0;
}

static __attribute__((noinline, noreturn)) void restart(void) {
  NVIC_SystemReset();
  while (1) { }
}

static __attribute__((noinline)) void wait_clock(uint32_t ready) {
  uint32_t timeout = 1000000;
  while (!(MDR_RST_CLK->CLOCK_STATUS & ready)) {
    if (!--timeout) restart();
  }
}

/* System Clock Initialization to 80 MHz */
static void clock_init(void) {
  /* Turn on HSE oscillator (8 MHz external crystal) */
  MDR_RST_CLK->HS_CONTROL |= RST_CLK_HSE_ON;
  wait_clock(RST_CLK_CLOCK_STATUS_HSE_RDY);

  /* Select HSE as CPU_PLL source (CPU_C1 = HSE) */
  MDR_RST_CLK->CPU_CLOCK = 0x02;

  /* Set CPU PLL multiplier to 10 (multiplier value in register is 9) and enable PLL */
  MDR_RST_CLK->PLL_CONTROL = (MDR_RST_CLK->PLL_CONTROL & ~0x0F00) | (9 << 8) | RST_CLK_PLL_CONTROL_PLL_CPU_ON;
  wait_clock(RST_CLK_CLOCK_STATUS_PLL_CPU_RDY);

  /* EEPROM Latency 3 (for CPU frequency up to 80 MHz) */
  MDR_EEPROM->CMD = EEPROM_Latency_3;

  /* Backup domain configuration (DUcc mode up to 80 MHz: SELECTRI=6, LOW=6) */
  MDR_BKP->REG_0E |= (6 | (6 << 3));

  /* Switch CPU clock to CPU_C3: CPU_C2 = PLL (bit 2), HCLK = CPU_C3 (bit 8), CPU_C1 = HSE (bit 1) */
  MDR_RST_CLK->CPU_CLOCK = (1 << 8) | (1 << 2) | 0x02;
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

/* Validate and jump to user application if valid */
static void jump_to_app(void) {
  uint32_t app_sp = *(volatile uint32_t *)MEM_APP_START;
  uint32_t app_pc = *(volatile uint32_t *)(MEM_APP_START + 4);

  if ((app_sp - 0x20000001u) < (32 * 1024) && !(app_sp & 3) &&
      (app_pc > MEM_APP_START) && (app_pc < (MEM_FLASH_START + MEM_FLASH_SIZE)) && (app_pc & 1)) {
    DFU_BTN_PORT->ANALOG = 0;
    DFU_BTN_PORT->PULL = 0;
    DFU_BTN_PORT->OE = 0;
    MDR_RST_CLK->PER_CLOCK &= ~RST_CLK_PCLK_PORTB;
    *(volatile uint32_t *)(0xE000ED08) = MEM_APP_START;
    __asm volatile (
      "cpsie i\n"
      "msr   msp, %0\n"
      "bx    %1\n"
      : : "r" (app_sp), "r" (app_pc) : "memory"
    );
    while (1) { }
  }
}

int main(void) {
  /* RAM functions are initialized before main. Apply the factory trims before
   * selecting the high-speed clock. */
  MDR_RST_CLK->PER_CLOCK |= RST_CLK_PCLK_EEPROM | RST_CLK_PCLK_BKP;
  MDR_BKP->REG_0E = (MDR_BKP->REG_0E & ~BKP_REG_0E_TRIM_Msk) |
      ((flash_read_info_word(EEPROM_FACTORY_DATA_LDO_TRIM) << BKP_REG_0E_TRIM_Pos) & BKP_REG_0E_TRIM_Msk);
  MDR_BKP->REG_0F =
      ((flash_read_info_word(EEPROM_FACTORY_DATA_HSI_TRIM) << BKP_REG_0F_HSI_TRIM_Pos) & BKP_REG_0F_HSI_TRIM_Msk) |
      ((flash_read_info_word(EEPROM_FACTORY_DATA_LSI_TRIM) << BKP_REG_0F_LSI_TRIM_Pos) & BKP_REG_0F_LSI_TRIM_Msk);
  MDR_EEPROM->ADR = 0;
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
  button_settle();

  /* Read the button state */
#if DFU_BTN_ACTIVE_LOW
  uint8_t btn_pressed = ((DFU_BTN_PORT->RXTX & DFU_BTN_PIN) == 0);
#else
  uint8_t btn_pressed = ((DFU_BTN_PORT->RXTX & DFU_BTN_PIN) != 0);
#endif

  /* Check DFU entry conditions */
  if (!btn_pressed && !rebooted_into_dfu()) {
    jump_to_app();
  }

  /* Configure DFU Status LED pin as Digital Output with Slow speed */
  DFU_LED_PORT->ANALOG |= DFU_LED_PIN;
  DFU_LED_PORT->OE |= DFU_LED_PIN;
  DFU_LED_PORT->PWR = (DFU_LED_PORT->PWR & ~(3 << (DFU_LED_PIN_POS * 2))) |
                      (1 << (DFU_LED_PIN_POS * 2)); /* Slow speed driver */

  /* Turn on/off LED initially */
#if DFU_LED_ACTIVE_HIGH
  DFU_LED_PORT->RXTX |= DFU_LED_PIN;
#else
  DFU_LED_PORT->RXTX &= ~DFU_LED_PIN;
#endif

  /* Initialize System and USB Clock */
  clock_init();

  USB_DFU_Init();
  usb_clock_init();

  /* Enable Interrupts globally for USB handling */
  __enable_irq();

  uint32_t blink_counter = 0;

  while (1) {
    if (dfu_reset_pending) {
      delay_ms(200); /* Allow the host to finish the status transfer. */
      USB_DevicePowerOff();
      delay_ms(10);
      restart();
    }

    delay_ms(1);
    blink_counter++;
    if (blink_counter >= 150) {
      blink_counter = 0;
      DFU_LED_PORT->RXTX ^= DFU_LED_PIN;
    }
  }
}
