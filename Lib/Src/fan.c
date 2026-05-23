//
// Created by 孙海涛 on 2026/5/1.
//

#include "fan.h"

#define SPEEDINTERVAL 200

uint32_t gaugeReading[4] = {0};

volatile uint32_t pulseCounter = 0;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == PulseIN_Pin) {
        pulseCounter++;
    }
}



void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    static uint8_t fanCursor = 0;
    if (htim == &htim2) {
        HAL_GPIO_TogglePin(WORKLED_GPIO_Port, WORKLED_Pin);

        // 读上一窗口的计数（< 2 视为浮空噪声，归 0）
        uint32_t cnt = pulseCounter;
        gaugeReading[fanCursor] = (cnt < 2) ? 0 : cnt;
        fanCursor = (fanCursor + 1) % 4;

        // 先切 mux 到下一个通道
        switch (fanCursor) {
            case 0:
                HAL_GPIO_WritePin(PinA_GPIO_Port, PinA_Pin, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(PinB_GPIO_Port, PinB_Pin, GPIO_PIN_RESET);
                break;
            case 1:
                HAL_GPIO_WritePin(PinA_GPIO_Port, PinA_Pin, GPIO_PIN_SET);
                HAL_GPIO_WritePin(PinB_GPIO_Port, PinB_Pin, GPIO_PIN_RESET);
                break;
            case 2:
                HAL_GPIO_WritePin(PinA_GPIO_Port, PinA_Pin, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(PinB_GPIO_Port, PinB_Pin, GPIO_PIN_SET);
                break;
            case 3:
                HAL_GPIO_WritePin(PinA_GPIO_Port, PinA_Pin, GPIO_PIN_SET);
                HAL_GPIO_WritePin(PinB_GPIO_Port, PinB_Pin, GPIO_PIN_SET);
                break;
        }

        // 切换后再清零 TIM4，切换瞬间产生的伪边沿被一并清掉
        pulseCounter = 0;
    }
}

uint32_t *getGaugeReading(void) {
    return gaugeReading;
}

void fanInit(void) {

    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
    HAL_TIM_Base_Start_IT(&htim2);
}

void fanLoop(uint8_t pwmDuty, uint8_t manualPWM, uint8_t *accRatio, uint8_t controlMode) {
    static uint32_t lastOpTime = 0;
    if (HAL_GetTick() - lastOpTime > SPEEDINTERVAL) {
        lastOpTime = HAL_GetTick();
        for (uint8_t i = 0; i < 4; i++) {
            if (controlMode == FAN_MODE_AUTO) {
                switch (i) {
                    case 0:
                        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pwmDuty + accRatio[i] * 10 >= 100 ? 100 : pwmDuty + accRatio[i] * 10);
                        break;
                    case 1:
                        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, pwmDuty + accRatio[i] * 10 >= 100 ? 100 : pwmDuty + accRatio[i] * 10);
                        break;
                    case 2:
                        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, pwmDuty + accRatio[i] * 10 >= 100 ? 100 : pwmDuty + accRatio[i] * 10);
                        break;
                    case 3:
                        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, pwmDuty + accRatio[i] * 10 >= 100 ? 100 : pwmDuty + accRatio[i] * 10);
                        break;
                }
            }
            else if (controlMode == FAN_MODE_MANUAL) {
                switch (i) {
                    case 0:
                        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, manualPWM + accRatio[i] * 10 >= 100 ? 100 : manualPWM + accRatio[i] * 10);
                        break;
                    case 1:
                        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, manualPWM + accRatio[i] * 10 >= 100 ? 100 : manualPWM + accRatio[i] * 10);
                        break;
                    case 2:
                        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, manualPWM + accRatio[i] * 10 >= 100 ? 100 : manualPWM + accRatio[i] * 10);
                        break;
                    case 3:
                        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, manualPWM + accRatio[i] * 10 >= 100 ? 100 : manualPWM + accRatio[i] * 10);
                        break;
                }
            }
        }
    }
}