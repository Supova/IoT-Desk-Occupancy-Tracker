#ifndef __BOOT_DESCRIPTOR_H
#define __BOOT_DESCRIPTOR_H

#include <stdint.h>
#include "stm32f4xx_hal.h"

#define BOOT_DESCRIPTOR_ADDR  0x0800C000
#define SLOT_VALID_MAGIC      0xAA55AA55
#define BOOT_FLASH_BLANK      0xFFFFFFFF

typedef enum {
    SLOT_A = 0,
    SLOT_B = 1
} boot_slot_t;

typedef enum{
    DESCRIPTOR_INVALID,
    DESCRIPTOR_BLANK,
    DESCRIPTOR_VALID

} descriptor_status_t;

/*
 * Boot descriptor stored at BOOT_DESCRIPTOR_ADDR (sector 3, 0x0800C000).
 * All members are uint32_t to match flash write granularity (word = 4 bytes).
 *
 * active_slot    - slot the bootloader jumps to on next boot (SLOT_A or SLOT_B)
 * slot_a_valid   - set to SLOT_VALID_MAGIC when slot A contains valid firmware
 * slot_b_valid   - set to SLOT_VALID_MAGIC when slot B contains valid firmware
 * boot_try_count - countdown for unconfirmed firmware; app writes 1 before reset,
 *                  bootloader decrements before jumping; rolls back to slot A if 0
 *                  and slot not yet confirmed
 * slot_confirmed - written to 1 by the app after MQTT connects successfully;
 *                  tells the bootloader the active slot is healthy
 */
typedef struct {
    uint32_t active_slot;
    uint32_t slot_a_valid;
    uint32_t slot_b_valid;
    uint32_t boot_try_count;
    uint32_t slot_confirmed;
} boot_descriptor_t;

void boot_descriptor_read(boot_descriptor_t *desc);
HAL_StatusTypeDef boot_descriptor_write(boot_descriptor_t *desc);
descriptor_status_t boot_descriptor_check_validity(boot_descriptor_t *desc);

 #endif /* __BOOT_DESCRIPTOR_H */