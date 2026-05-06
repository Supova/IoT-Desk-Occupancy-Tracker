/*
 * ota_flash.h
 *
 *  Created on: Feb 18, 2025
 *      Author: Shreyas Acharya, BHARATI SOFTWARE
 */

#ifndef INC_OTA_FLASH_H_
#define INC_OTA_FLASH_H_

#include "main.h"

typedef enum
{
    FLASH_OK = 0,
    FLASH_ERROR
} flash_status_t;

flash_status_t flash_write(__IO uint32_t* Address, uint32_t* Data, uint16_t DataLength);
flash_status_t flash_erase(uint32_t start_address);

#endif /* INC_OTA_FLASH_H_ */
