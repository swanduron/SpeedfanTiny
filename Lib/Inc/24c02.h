//
// Created by 孙海涛 on 2026/5/5.
//

#ifndef SPEEDFANTINY_24C02_H
#define SPEEDFANTINY_24C02_H

#include "i2c.h"

HAL_StatusTypeDef eepromWrite(uint8_t *pData);
HAL_StatusTypeDef eepromRead(uint8_t *pData);
void eepromInit(void);
#endif //SPEEDFANTINY_24C02_H
