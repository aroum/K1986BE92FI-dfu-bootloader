#ifndef __REBOOT_H
#define __REBOOT_H

#include <stdint.h>
#include "config.h"

/* The address at the very end of RAM (last 8 bytes of 32 KB SRAM) */
#define REBOOT_MAGIC_ADDR    (0x20000000 + 32 * 1024 - 8)

static inline int rebooted_into_dfu(void) {
    volatile uint64_t *magic_ptr = (volatile uint64_t *)REBOOT_MAGIC_ADDR;
    if (*magic_ptr == REBOOT_DFU_MAGIC) {
        /* Clear the magic value to prevent booting into DFU indefinitely */
        *magic_ptr = 0;
        return 1;
    }
    return 0;
}

static inline void request_dfu_reboot(void) {
    volatile uint64_t *magic_ptr = (volatile uint64_t *)REBOOT_MAGIC_ADDR;
    *magic_ptr = REBOOT_DFU_MAGIC;
}

#endif /* __REBOOT_H */
