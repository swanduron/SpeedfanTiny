//
// Created by 孙海涛 on 2026/5/1.
//

#ifndef SPEEDFANTINY_FAN_H
#define SPEEDFANTINY_FAN_H

#include "tim.h"

#define FAN_MODE_AUTO   0
#define FAN_MODE_MANUAL 1

void fanInit(void);
void fanLoop(uint8_t pwmDuty, uint8_t manualPWM, uint8_t *accRatio, uint8_t controlMode);
uint32_t *getGaugeReading(void);
#endif //SPEEDFANTINY_FAN_H
