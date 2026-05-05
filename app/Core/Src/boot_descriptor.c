#include "boot_descriptor.h"
#include "stm32f4xx_hal.h"

void boot_descriptor_read(boot_descriptor_t *desc) {
	*desc = *(boot_descriptor_t *)BOOT_DESCRIPTOR_ADDR;
}

HAL_StatusTypeDef boot_descriptor_write(boot_descriptor_t *desc) {
	FLASH_EraseInitTypeDef erase_init = {
	    .TypeErase = FLASH_TYPEERASE_SECTORS,
	    .Sector = FLASH_SECTOR_3,
	    .NbSectors = 1,
	    .VoltageRange = FLASH_VOLTAGE_RANGE_3
	};

	uint32_t sector_error = 0;
	HAL_StatusTypeDef status;

	HAL_FLASH_Unlock();

	status = HAL_FLASHEx_Erase(&erase_init, &sector_error);
	if (status != HAL_OK) {
		HAL_FLASH_Lock();
		return status;
	}

	uint32_t *data = (uint32_t *)desc;
	uint32_t addr = BOOT_DESCRIPTOR_ADDR;
	uint32_t words = sizeof(boot_descriptor_t) / sizeof(uint32_t);

	for (uint32_t i = 0; i < words; i++) {
		status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, data[i]);
		if (status != HAL_OK) {
			HAL_FLASH_Lock();
			return status;
		}
		addr += 4;
	}

	HAL_FLASH_Lock();
	return HAL_OK;
}

static uint8_t descriptor_is_blank(boot_descriptor_t *desc) {
    return desc->active_slot    == BOOT_FLASH_BLANK &&
           desc->slot_a_valid   == BOOT_FLASH_BLANK &&
           desc->slot_b_valid   == BOOT_FLASH_BLANK &&
           desc->boot_try_count == BOOT_FLASH_BLANK &&
           desc->slot_confirmed == BOOT_FLASH_BLANK;
}

descriptor_status_t boot_descriptor_check_validity(boot_descriptor_t *desc) {
     if (descriptor_is_blank(desc)) {                                        
      return DESCRIPTOR_BLANK;
  }

  if (desc->slot_a_valid == SLOT_VALID_MAGIC || desc->slot_b_valid ==
  SLOT_VALID_MAGIC) {
      return DESCRIPTOR_VALID;
  }

  return DESCRIPTOR_INVALID;
}
