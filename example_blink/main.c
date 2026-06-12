#include "MDR32FxQI_bkp.h"
#include "MDR32FxQI_eeprom.h"
#include "MDR32FxQI_port.h"
#include "MDR32FxQI_rst_clk.h"
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

int main(void) {
  /* Set Vector Table Offset to point to our application at 0x08002000 */
  *(volatile uint32_t *)(0xE000ED08) = 0x08002000;

  /* Initialize System Clock */
  clock_init();

  /* Enable PORTB Clock for LED (PB7) */
  RST_CLK_PCLKcmd(RST_CLK_PCLK_PORTB, ENABLE);

  /* Configure LED pin (PB7) as Output */
  PORT_InitTypeDef PORT_InitStructure;
  PORT_StructInit(&PORT_InitStructure);
  PORT_InitStructure.PORT_Pin = PORT_Pin_7;
  PORT_InitStructure.PORT_OE = PORT_OE_OUT;
  PORT_InitStructure.PORT_FUNC = PORT_FUNC_PORT;
  PORT_InitStructure.PORT_MODE = PORT_MODE_DIGITAL;
  PORT_InitStructure.PORT_SPEED = PORT_SPEED_SLOW;
  PORT_Init(MDR_PORTB, &PORT_InitStructure);

  while (1) {
    /* Toggle LED PB7 */
    PORT_SetBits(MDR_PORTB, PORT_Pin_7);
    delay_us(500000); /* 500 ms */

    PORT_ResetBits(MDR_PORTB, PORT_Pin_7);
    delay_us(500000); /* 500 ms */
  }
}
