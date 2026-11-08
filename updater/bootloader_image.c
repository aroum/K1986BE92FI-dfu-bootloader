#include <stdint.h>

__attribute__((section(".rodata.bootloader_bin")))
const uint8_t bootloader_bin[] = {
#include "bootloader_image.inc"
};

const uint32_t bootloader_bin_size = sizeof(bootloader_bin);
