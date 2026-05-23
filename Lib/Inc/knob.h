//
// Created by 孙海涛 on 2026/5/1.
//

#ifndef SPEEDFANTINY_KNOB_H
#define SPEEDFANTINY_KNOB_H

#include "tim.h"

typedef void (*knobCallback)(void);

void knobInit(void);
void knobLoop(void);
void setForwareAction(knobCallback callback);
void setBackwardAction(knobCallback callback);
void setPressedAction(knobCallback callback);


#endif //SPEEDFANTINY_KNOB_H
