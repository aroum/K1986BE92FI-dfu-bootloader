#include "flash.h"
#include "MDR32FxQI_eeprom.h"
#include "MDR32FxQI_rst_clk.h"
#include "MDR32FxQI_config.h"

void flash_init(void) {
    /* Enable EEPROM clock */
    RST_CLK_PCLKcmd(RST_CLK_PCLK_EEPROM, ENABLE);
    /* Set EEPROM latency for high CPU frequency (Latency 3 matches up to 80 MHz) */
    EEPROM_SetLatency(EEPROM_Latency_3);
}

/* 
 * Erase a page of the flash memory.
 * For Milandr, this function must be in RAM to avoid read-while-write hazards.
 */
__RAMFUNC void flash_erase_page(uint32_t address) {
    __disable_irq();
    EEPROM_ErasePage(address, EEPROM_Main_Bank_Select);
    EEPROM_UpdateDCache();
    __enable_irq();
}

/* 
 * Write a 32-bit word to flash.
 * For Milandr, this function must be in RAM to avoid read-while-write hazards.
 */
__RAMFUNC void flash_write_word(uint32_t address, uint32_t data) {
    __disable_irq();
    EEPROM_ProgramWord(address, EEPROM_Main_Bank_Select, data);
    EEPROM_UpdateDCache();
    __enable_irq();
}
