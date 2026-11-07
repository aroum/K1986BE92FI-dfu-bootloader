#ifndef __FLASH_H
#define __FLASH_H

#include <stdint.h>
#include "config.h"
#include "MDR32FxQI_eeprom.h"

uint32_t flash_read_info_word(uint32_t address);

/* Erase a single page of flash containing the given address in the selected bank */
void flash_erase_page(uint32_t address, EEPROM_Mem_Bank bank);

/* Write a 32-bit word to the specified address in flash */
void flash_write_word(uint32_t address, uint32_t data, EEPROM_Mem_Bank bank);

/* Read a block of memory to buffer (RAM-safe for Info EEPROM) */
void flash_read_block(uint32_t address, uint8_t *buffer, uint32_t length, EEPROM_Mem_Bank bank);

#endif /* __FLASH_H */
