//
// Created by 孙海涛 on 2026/5/5.
//

#include "24c02.h"

#define EEPROM_ADDR (0x50 << 1)
#define PAGE_SIZE   8
#define OPERATION_INTERVAL 10

uint32_t eepromOPTimeStamp = 0;

/* EEPROM format
Byte0: Mode. 0x00: Auto, 0x01: Manual
Byte1: Upper temperature threshold (Auto mode)
Byte2: Lower temperature threshold (Auto mode)
Byte3: Duty in Manual mode
Byte4: Accelerate for Fan1 (high 4bit) and Fan2 (low 4bit)
Byte5: Accelerate for Fan3 (high 4bit) and Fan4 (low 4bit)
Byte6: Reserved
Byte7: Magic number (0xA5)
 */

void eepromInit(void) {
    HAL_Delay(40);
}


HAL_StatusTypeDef eepromWrite(uint8_t *pData) {

    uint32_t currentTime = HAL_GetTick();
    if (currentTime - eepromOPTimeStamp > OPERATION_INTERVAL) {
        HAL_StatusTypeDef writeStatus = HAL_I2C_Mem_Write(&hi2c1, EEPROM_ADDR, 0x00,
            I2C_MEMADD_SIZE_8BIT, pData, 8, 100);
        if (writeStatus == HAL_OK) {
            eepromOPTimeStamp = HAL_GetTick();
        }
        return writeStatus;
    }
    return HAL_BUSY;
}



HAL_StatusTypeDef eepromRead(uint8_t *pData) {
    uint32_t currentTime = HAL_GetTick();
    if (currentTime - eepromOPTimeStamp > OPERATION_INTERVAL) {
        HAL_StatusTypeDef readStatus = HAL_I2C_Mem_Read(&hi2c1, EEPROM_ADDR, 0x00,
            I2C_MEMADD_SIZE_8BIT, pData, 8, 100);
        return readStatus;
    }
    return HAL_BUSY;
}

