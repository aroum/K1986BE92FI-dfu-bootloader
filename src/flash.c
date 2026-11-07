#include "flash.h"
#include "MDR32FxQI_eeprom.h"
#include "MDR32FxQI_rst_clk.h"
#include "MDR32FxQI_config.h"

__RAMFUNC __attribute__((noinline)) uint32_t flash_read_info_word(uint32_t address) {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    uint32_t command = (MDR_EEPROM->CMD & EEPROM_CMD_DELAY_Msk) | EEPROM_CMD_CON | EEPROM_CMD_IFREN;
    MDR_EEPROM->KEY = 0x8AAA5551;
    MDR_EEPROM->CMD = command;
    MDR_EEPROM->ADR = address;
    MDR_EEPROM->CMD = command | EEPROM_CMD_XE | EEPROM_CMD_YE | EEPROM_CMD_SE;
    MDR_EEPROM->DO; MDR_EEPROM->DO; MDR_EEPROM->DO;
    uint32_t data = MDR_EEPROM->DO;
    MDR_EEPROM->CMD = command & EEPROM_CMD_DELAY_Msk;
    MDR_EEPROM->KEY = 0;
    __set_PRIMASK(primask);
    return data;
}

/* 
 * Erase a page of the flash memory.
 * For Milandr, this function must be in RAM to avoid read-while-write hazards.
 */
__RAMFUNC __attribute__((noinline)) void flash_erase_page(uint32_t address, EEPROM_Mem_Bank bank) {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    EEPROM_ErasePage(address, bank);
    EEPROM_UpdateDCache();
    __set_PRIMASK(primask);
}

/* 
 * Write a 32-bit word to flash.
 * For Milandr, this function must be in RAM to avoid read-while-write hazards.
 */
__RAMFUNC __attribute__((noinline)) void flash_write_word(uint32_t address, uint32_t data, EEPROM_Mem_Bank bank) {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    EEPROM_ProgramWord(address, bank, data);
    EEPROM_UpdateDCache();
    __set_PRIMASK(primask);
}

void flash_read_block(uint32_t address, uint8_t *buffer, uint32_t length, EEPROM_Mem_Bank bank) {
    uint32_t *dst = (uint32_t *)(void *)buffer;
    for (uint32_t i = 0; i < length; i += 4) {
        *dst++ = (bank == EEPROM_Main_Bank_Select) ?
            *(volatile uint32_t *)(address + i) :
            flash_read_info_word(address + i);
    }
}
