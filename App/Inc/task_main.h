//
// Created by 孙海涛 on 2026/5/1.
//

#ifndef SPEEDFANTINY_TASK_MAIN_H
#define SPEEDFANTINY_TASK_MAIN_H

#include "usart.h"
#include "knob.h"
#include "i2c.h"
#include "fan.h"
#include "ds18b20.h"
#include "24c02.h"
#include <stdio.h>
#include <string.h>

void taskInit(void);
void taskLoop(void);

typedef enum {OK, MEMREADERROR, MEMWRITEERROR, DISPALYERROR} ErrorType;

#endif //SPEEDFANTINY_TASK_MAIN_H
