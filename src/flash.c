#include "flash.h"
#include "MDR32FxQI_eeprom.h"
#include "MDR32FxQI_rst_clk.h"
#include "MDR32FxQI_config.h"

void flash_init(void) {
    /* Enable EEPROM clock */
    MDR_RST_CLK->PER_CLOCK |= RST_CLK_PCLK_EEPROM;
    /* Set EEPROM latency for high CPU frequency (Latency 3 matches up to 80 MHz) */
    MDR_EEPROM->CMD = EEPROM_Latency_3;
}

/* 
 * Erase a page of the flash memory.
 * For Milandr, this function must be in RAM to avoid read-while-write hazards.
 */
__RAMFUNC __attribute__((noinline)) void flash_erase_page(uint32_t address, EEPROM_Mem_Bank bank) {
    __disable_irq();
    EEPROM_ErasePage(address, bank);
    EEPROM_UpdateDCache();
    __enable_irq();
}

/* 
 * Write a 32-bit word to flash.
 * For Milandr, this function must be in RAM to avoid read-while-write hazards.
 */
__RAMFUNC __attribute__((noinline)) void flash_write_word(uint32_t address, uint32_t data, EEPROM_Mem_Bank bank) {
    __disable_irq();
    EEPROM_ProgramWord(address, bank, data);
    EEPROM_UpdateDCache();
    __enable_irq();
}

__RAMFUNC __attribute__((noinline)) void flash_read_block(uint32_t address, uint8_t *buffer, uint32_t length, EEPROM_Mem_Bank bank) {
    if (bank == EEPROM_Main_Bank_Select) {
        const uint8_t *src = (const uint8_t *)address;
        for (uint32_t i = 0; i < length; i++) {
            buffer[i] = src[i];
        }
    } else {
        const uint8_t *src = (const uint8_t *)(address & 0x0FFF);
        for (uint32_t i = 0; i < length; i++) {
            buffer[i] = src[i];
        }
    }
}
