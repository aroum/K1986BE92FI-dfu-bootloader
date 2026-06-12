#ifndef __FLASH_H
#define __FLASH_H

#include <stdint.h>
#include "config.h"

/* Initialize Flash controller for programming/erasing */
void flash_init(void);

/* Erase a single page of flash containing the given address */
void flash_erase_page(uint32_t address);

/* Write a 32-bit word to the specified address in flash */
void flash_write_word(uint32_t address, uint32_t data);

#endif /* __FLASH_H */
