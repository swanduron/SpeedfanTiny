//
// Created by 孙海涛 on 2026/5/1.
//

#include "../Inc/knob.h"

#define ENCODER_INIT_VALUE (65535 / 2)
#define DEBOUNCE_TIME 10


typedef enum {Pressed, Unpressed} BtnStatus;


knobCallback forwardCallback = NULL;
knobCallback backwardCallback = NULL;
knobCallback pressedCallback = NULL;

void setForwareAction(knobCallback callback) {
    forwardCallback = callback;
}

void setBackwardAction(knobCallback callback) {
    backwardCallback = callback;
}

void setPressedAction(knobCallback callback) {
    pressedCallback = callback;
}

void knobInit(void) {
    HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_ALL);
    __HAL_TIM_SET_COUNTER(&htim1,ENCODER_INIT_VALUE);
}

void knobLoop(void) {
    uint32_t counter = __HAL_TIM_GET_COUNTER(&htim1);
    if (counter > ENCODER_INIT_VALUE) {
        if (forwardCallback != NULL) {
            forwardCallback();
        }
    }
    else if (counter < ENCODER_INIT_VALUE) {
        if (backwardCallback != NULL) {
            backwardCallback();
        }
    }
    __HAL_TIM_SET_COUNTER(&htim1,ENCODER_INIT_VALUE);
    static uint32_t btnOpTime = 0;
    static uint8_t btnOpState = 0;
    BtnStatus btnStatus = HAL_GPIO_ReadPin(KnobBtn_GPIO_Port, KnobBtn_Pin) == GPIO_PIN_RESET ? Pressed : Unpressed;
    if (btnStatus == Pressed) {
        if (btnOpTime == 0) {
            btnOpTime = HAL_GetTick();
        }
        if (btnOpState == 0 && HAL_GetTick() - btnOpTime > DEBOUNCE_TIME) {
            if (pressedCallback != NULL) {
                pressedCallback();
            }
            btnOpState = 1;
        }
    }
    else {
        btnOpState = 0;
        btnOpTime = 0;
    }
}