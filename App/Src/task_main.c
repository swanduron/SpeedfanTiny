//
// Created by 孙海涛 on 2026/5/1.
//

#include "task_main.h"

#include "oled.h"

#define FANGAUGEMULTIPLE   60
#define IDLE_TIMEOUT_MS    5000U
#define OLED_CONTRAST_NORM 0xDFU
#define OLED_CONTRAST_DIM  0x00U

uint32_t gauges[4];

char messageBuffer[40];
ErrorType errorType = OK;
uint8_t cursorPosition = 0;

// System config
/* EEPROM format
Byte0: Mode. 0x00: Auto, 0x01: Manual
Byte1: Upper temperature threshold (Auto mode)
Byte2: Lower temperature threshold (Auto mode)
Byte3: Duty in Manual mode
Byte4: Accelerate for Fan1 (high 4bit) and Fan2 (low 4bit)
Byte5: Accelerate for Fan3 (high 4bit) and Fan4 (low 4bit)
Byte6: Reserved
Byte7: Magic number (0xA5). If not matched, defaults are loaded and saved.
 */

#define EEPROM_MAGIC          0xA5U
#define DEFAULT_CONTROL_MODE  ControlAuto
#define DEFAULT_UP_TEMP       60U
#define DEFAULT_DOWN_TEMP     40U
#define DEFAULT_MANUAL_DUTY   50U

uint8_t sysConfig[8];
uint8_t sysConfigShadow[8];

uint8_t accRatio[4] = {1, 1, 1, 1};
uint8_t upTemp = DEFAULT_UP_TEMP;
uint8_t downTemp = DEFAULT_DOWN_TEMP;
uint8_t manualDuty = DEFAULT_MANUAL_DUTY;

float temperature = 0.0f;

typedef enum {ModeNormal, ModeSetting} SystemMode;
SystemMode systemMode = ModeNormal;


typedef enum {ControlAuto, ControlManual} ControlMode;
ControlMode controlMode = ControlAuto;

typedef struct {uint8_t x1; uint8_t y1; uint8_t x2; uint8_t y2;} Position;
Position autoPosition[7] = {
    {74, 4, 122, 4},
    {0, 52, 17, 52},
    {18, 52, 35, 52},
    {36, 52, 53, 52},
    {54, 52, 71, 52},
    {85, 43, 102, 43},
    {85, 52, 102, 52}
};
Position manualPosition[6] = {
    {74, 4, 122, 4},
    {0, 52, 17, 52},
    {18, 52, 35, 52},
    {36, 52, 53, 52},
    {54, 52, 71, 52},
    {89, 48, 113, 48},
};

uint8_t I2C_ScanFirstDevice(I2C_HandleTypeDef *hi2c)
{
    for (uint8_t addr = 1; addr < 128; addr++) {
        if (HAL_I2C_IsDeviceReady(hi2c, (uint16_t)(addr << 1), 1, 10) == HAL_OK) {
            return addr;
        }
    }
    return 0x00;
}

uint8_t pwmDutyCal(uint8_t high, uint8_t low, float temp) {
    uint8_t intTemp = (uint8_t)temp;
    if (intTemp > high) {
        return 100;
    }
    else if (intTemp < low) {
        return 0;
    }
    else {
        return (uint8_t)((intTemp - low) * 100 / (high - low));
    }
}

static uint32_t s_lastActivityTime = 0;
static uint8_t  s_isIdle           = 0;

// Forward declaration — defined after action callbacks
void sysConfigCmp(uint8_t *p, uint8_t *pShadow);

static void resetIdleTimer(void) {
    s_lastActivityTime = HAL_GetTick();
    if (s_isIdle) {
        s_isIdle = 0;
        OLED_SetContrast(OLED_CONTRAST_NORM);
    }
}

void idleOperation(void) {
    if (!s_isIdle && (HAL_GetTick() - s_lastActivityTime > IDLE_TIMEOUT_MS)) {
        s_isIdle = 1;
        OLED_SetContrast(OLED_CONTRAST_DIM);
        if (systemMode == ModeSetting) {
            systemMode = ModeNormal;
            sysConfigCmp(sysConfig, sysConfigShadow);
        }
    }
}

void sysConfigCmp(uint8_t *p, uint8_t *pShadow) {
    p[0] = controlMode == ControlAuto ? 0x00 : 0x01;
    p[1] = upTemp;
    p[2] = downTemp;
    p[3] = manualDuty;
    p[4] = (accRatio[0] << 4) + accRatio[1];
    p[5] = (accRatio[2] << 4) + accRatio[3];
    p[6] = 0x00U;
    p[7] = EEPROM_MAGIC;

    if (memcmp(pShadow, p, 8) != 0) {
        memcpy(pShadow, p, 8);
        HAL_StatusTypeDef halStatus = eepromWrite(p);
        if (halStatus != HAL_OK) {
            errorType = MEMWRITEERROR;
        }
    }
}

void forwardAction() {
    resetIdleTimer();
    if (systemMode == ModeSetting) {
        switch (cursorPosition) {
        case 0:
            if (controlMode == ControlManual) {
                controlMode = ControlAuto;
            }
            else {
                controlMode = ControlManual;
            }
            break;
        case 1:
            accRatio[0] ++;
            if (accRatio[0] > 9) {
                accRatio[0] = 9;
            }
            break;
        case 2:
            accRatio[1] ++;
            if (accRatio[1] > 9) {
                accRatio[1] = 9;
            }
            break;
        case 3:
            accRatio[2] ++;
            if (accRatio[2] > 9) {
                accRatio[2] = 9;
            }
            break;
        case 4:
            accRatio[3] ++;
            if (accRatio[3] > 9) {
                accRatio[3] = 9;
            }
            break;
        case 5:
            if (controlMode == ControlAuto) {
                upTemp += 5;
                if (upTemp > 85) {
                    upTemp = 85;
                }
            } else {
                manualDuty += 5;
                if (manualDuty > 100) {
                    manualDuty = 100;
                }
            }
            break;
        case 6:
            downTemp += 5;
            if (downTemp >= upTemp) {
                downTemp = upTemp - 5;
            }
            break;
        }
    }
}

void backwardAction() {
    resetIdleTimer();
    if (systemMode == ModeSetting) {
        switch (cursorPosition) {
            case 0:
                if (controlMode == ControlManual) {
                    controlMode = ControlAuto;
                }
                else {
                    controlMode = ControlManual;
                }
                break;
            case 1:
                accRatio[0]--;
                if (accRatio[0] > 250) {
                    accRatio[0] = 0;
                }
                break;
            case 2:
                accRatio[1]--;
                if (accRatio[1] > 250) {
                    accRatio[1] = 0;
                }
                break;
            case 3:
                accRatio[2]--;
                if (accRatio[2] > 250) {
                    accRatio[2] = 0;
                }
                break;
            case 4:
                accRatio[3]--;
                if (accRatio[3] > 250) {
                    accRatio[3] = 0;
                }
                break;
            case 5:
                if (controlMode == ControlAuto) {
                    upTemp = (upTemp >= 55 + 5) ? (upTemp - 5) : 55;
                } else {
                    manualDuty = (manualDuty >= 5) ? (manualDuty - 5) : 0;
                }
                break;
            case 6:
                downTemp = (downTemp >= 10 + 5) ? (downTemp - 5) : 10;
                break;
        }
    }
}


void pressedAction() {
    resetIdleTimer();
    if (systemMode == ModeNormal) {
        systemMode = ModeSetting;
        cursorPosition = 0;
    }
    else {
        if (controlMode == ControlAuto) {
            cursorPosition++;
            if (cursorPosition > 6) {
                systemMode = ModeNormal;
                sysConfigCmp(sysConfig, sysConfigShadow);
            }
        }
        else if (controlMode == ControlManual) {
            cursorPosition++;
            if (cursorPosition > 5) {
                systemMode = ModeNormal;
                sysConfigCmp(sysConfig, sysConfigShadow);
            }
        }
    }
}

static void applyDefaultConfig(void) {
    controlMode  = DEFAULT_CONTROL_MODE;
    upTemp       = DEFAULT_UP_TEMP;
    downTemp     = DEFAULT_DOWN_TEMP;
    manualDuty   = DEFAULT_MANUAL_DUTY;
    accRatio[0]  = 1;
    accRatio[1]  = 1;
    accRatio[2]  = 1;
    accRatio[3]  = 1;
}

void loadSysConfig(uint8_t *p, uint8_t *pShadow) {
    HAL_StatusTypeDef halStatus = eepromRead(p);
    if (halStatus == HAL_OK) {
        if (p[7] != EEPROM_MAGIC) {
            // First boot or corrupted data: apply defaults and persist them
            applyDefaultConfig();
            sysConfigCmp(p, pShadow);
            return;
        }
        memcpy(pShadow, p, 8);
        controlMode  = p[0];
        upTemp       = p[1];
        downTemp     = p[2];
        manualDuty   = p[3];
        accRatio[0]  = p[4] >> 4;
        accRatio[1]  = p[4] & 0x0FU;
        accRatio[2]  = p[5] >> 4;
        accRatio[3]  = p[5] & 0x0FU;
    }
    else {
        errorType = MEMREADERROR;
    }
}




void taskInit(void) {
    knobInit();
    fanInit();
    ds18b20Init();
    eepromInit(); // if power on is too fast, the EEPROM will not be ready when the 1st write/read
    setForwareAction(forwardAction);
    setBackwardAction(backwardAction);
    setPressedAction(pressedAction);
    OLED_Init();
    loadSysConfig(sysConfig, sysConfigShadow);
}


void mainFrameDrawer() {
    sprintf(messageBuffer, "%.2f", temperature);
    OLED_PrintASCIIString(0, 0, messageBuffer, &afont16x12, OLED_COLOR_NORMAL);
    uint8_t charOffset = controlMode == ControlManual ? 84 : 90;
    OLED_PrintASCIIString(charOffset,4, controlMode == ControlAuto ? "AUTO" : "MANUAL", &afont8x6, OLED_COLOR_NORMAL);
    OLED_DrawLine(0, 18, 127, 18, OLED_COLOR_NORMAL);
    sprintf(messageBuffer, "FAN1:%05lu FAN2:%05lu", (unsigned long)(gauges[0] * FANGAUGEMULTIPLE), (unsigned long)(gauges[1] * FANGAUGEMULTIPLE));
    OLED_PrintASCIIString(0, 21, messageBuffer, &afont8x6, OLED_COLOR_NORMAL);
    sprintf(messageBuffer, "FAN3:%05lu FAN4:%05lu", (unsigned long)(gauges[2] * FANGAUGEMULTIPLE), (unsigned long)(gauges[3] * FANGAUGEMULTIPLE));
    OLED_PrintASCIIString(0, 30, messageBuffer, &afont8x6, OLED_COLOR_NORMAL);
    OLED_DrawLine(0, 39, 127, 39, OLED_COLOR_NORMAL);

    sprintf(messageBuffer, "A1 A2 A3 A4");
    OLED_PrintASCIIString(5, 43, messageBuffer, &afont8x6, OLED_COLOR_NORMAL);
    sprintf(messageBuffer, "%02d %02d %02d %02d", accRatio[0] * 10, accRatio[1] * 10,
        accRatio[2] * 10, accRatio[3] * 10);
    OLED_PrintASCIIString(5, 52, messageBuffer, &afont8x6, OLED_COLOR_NORMAL);

    OLED_DrawLine(77, 39, 77, 63, OLED_COLOR_NORMAL);
    OLED_DrawLine(0, 63, 127, 63, OLED_COLOR_NORMAL);
}

void auxDataDrawer() {
    if (controlMode == ControlAuto) {
        OLED_DrawImage(79, 43, &uparrowImg, OLED_COLOR_NORMAL);
        OLED_DrawImage(79, 52, &downarrowImg, OLED_COLOR_NORMAL);
        sprintf(messageBuffer, "%02d", upTemp);
        OLED_PrintASCIIString(90, 43, messageBuffer, &afont8x6, OLED_COLOR_NORMAL);
        sprintf(messageBuffer, "%02d", downTemp);
        OLED_PrintASCIIString(90, 52, messageBuffer, &afont8x6, OLED_COLOR_NORMAL);
        uint8_t autoPWM = pwmDutyCal(upTemp, downTemp, temperature);
        sprintf(messageBuffer, "%03d", autoPWM);
        OLED_PrintASCIIString(108, 47, messageBuffer, &afont8x6, OLED_COLOR_NORMAL);
    }
    else {
        sprintf(messageBuffer, "%03d", manualDuty);
        OLED_PrintASCIIString(95,48, messageBuffer, &afont8x6, OLED_COLOR_NORMAL);
    }
}

void cursorDrawer() {

    if (controlMode == ControlAuto) {
        OLED_PrintASCIIChar(autoPosition[cursorPosition].x1, autoPosition[cursorPosition].y1, '[', &afont8x6, OLED_COLOR_NORMAL);
        OLED_PrintASCIIChar(autoPosition[cursorPosition].x2, autoPosition[cursorPosition].y2, ']', &afont8x6, OLED_COLOR_NORMAL);
    }
    else if (controlMode == ControlManual) {
        OLED_PrintASCIIChar(manualPosition[cursorPosition].x1, manualPosition[cursorPosition].y1, '[', &afont8x6, OLED_COLOR_NORMAL);
        OLED_PrintASCIIChar(manualPosition[cursorPosition].x2, manualPosition[cursorPosition].y2, ']', &afont8x6, OLED_COLOR_NORMAL);
    }
}

void taskLoop(void) {
    knobLoop();
    idleOperation();
    temperature = ds18b20Read();

    fanLoop(pwmDutyCal(upTemp, downTemp, temperature), manualDuty, accRatio, controlMode);
    memcpy(gauges, getGaugeReading(), sizeof(gauges));

    OLED_NewFrame();
    if (systemMode == ModeNormal) {
        mainFrameDrawer();
        auxDataDrawer();
    }
    else if (systemMode == ModeSetting) {
        mainFrameDrawer();
        auxDataDrawer();
        cursorDrawer();
    }
    OLED_ShowFrame();
}