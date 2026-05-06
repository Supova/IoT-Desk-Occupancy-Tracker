/*
 * ota_flash.c
 *
 *  Created on: Feb 18, 2025
 *      Author: Shreyas Acharya, BHARATI SOFTWARE
 *  Modified by Supova: flash_erase derives sector from address for A/B OTA
 */

#include "ota_flash.h"
#include "application_config.h"

/**
 * @brief Erases the flash sector at the given start address.
 * @param start_address  Must be USER_FLASH_FIRST_SECTOR_ADDRESS or
 *                       USER_FLASH_SECOND_SECTOR_ADDRESS.
 * @retval FLASH_OK on success, FLASH_ERROR otherwise.
 */
flash_status_t flash_erase(uint32_t start_address) {
	FLASH_EraseInitTypeDef flash_erase_init;
	uint32_t sector_error = 0;

	flash_erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
	flash_erase_init.NbSectors = 1;
	flash_erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    // A/B slot change
	if (start_address == USER_FLASH_FIRST_SECTOR_ADDRESS) {
		flash_erase_init.Sector = FLASH_SECTOR_5;
	} else if (start_address == USER_FLASH_SECOND_SECTOR_ADDRESS) {
		flash_erase_init.Sector = FLASH_SECTOR_6;
	} else {
		return FLASH_ERROR;
	}

	HAL_FLASH_Unlock();

	flash_status_t status = FLASH_OK;
	if (HAL_FLASHEx_Erase(&flash_erase_init, &sector_error) != HAL_OK) {
		status = FLASH_ERROR;
	}

	HAL_FLASH_Lock();
	return status;
}

/**
 * @brief Writes a word-aligned data buffer to flash.
 * @param flash_address  Pointer to current write address (advanced by function).
 * @param data           Pointer to data buffer (32-bit words).
 * @param data_length    Number of 32-bit words to write.
 * @retval FLASH_OK on success, FLASH_ERROR otherwise.
 */
flash_status_t flash_write(__IO uint32_t *flash_address, uint32_t *data, uint16_t data_length) {
	flash_status_t status = FLASH_OK;

	HAL_FLASH_Unlock();

	for (uint32_t i = 0; (i < data_length) && (*flash_address <= (USER_FLASH_END_ADDRESS - 4)); i++) {
		if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, *flash_address, *(uint32_t *)(data + i)) == HAL_OK) {
			*flash_address += 4;
		} else {
			status = FLASH_ERROR;
			break;
		}
	}

	HAL_FLASH_Lock();
	return status;
}
