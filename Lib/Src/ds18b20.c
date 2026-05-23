//
// Created by 孙海涛 on 2026/5/5.
//

#include "ds18b20.h"
#include "main.h"

// ---------------------------------------------------------------------------
// Configurable options — adjust when reusing this driver on another project
// ---------------------------------------------------------------------------
#define DS18B20_RES_9BIT         0x1FU   // conversion time:  93.75 ms
#define DS18B20_RES_10BIT        0x3FU   // conversion time: 187.5  ms
#define DS18B20_RES_11BIT        0x5FU   // conversion time: 375    ms
#define DS18B20_RES_12BIT        0x7FU   // conversion time: 750    ms

#define DS18B20_RESOLUTION       DS18B20_RES_12BIT
#define DS18B20_CONVERT_TIME_MS  850U
#define DS18B20_ERROR_VALUE      99.99f
#define DS18B20_MAX_CRC_ERRORS   3U   // consecutive CRC failures before permanent error
// ---------------------------------------------------------------------------

// 1-Wire bus helpers (open-drain: HIGH releases the line; IDR readable in output mode)
#define BUS_LOW()   HAL_GPIO_WritePin(DS18B20_GPIO_Port, DS18B20_Pin, GPIO_PIN_RESET)
#define BUS_HIGH()  HAL_GPIO_WritePin(DS18B20_GPIO_Port, DS18B20_Pin, GPIO_PIN_SET)
#define BUS_READ()  HAL_GPIO_ReadPin (DS18B20_GPIO_Port, DS18B20_Pin)

// DS18B20 commands
#define CMD_SKIP_ROM          0xCCU
#define CMD_CONVERT_T         0x44U
#define CMD_READ_SCRATCHPAD   0xBEU
#define CMD_WRITE_SCRATCHPAD  0x4EU

// Module state
static float    s_temperature   = 0.0f;
static uint32_t s_convStartTime = 0U;
static uint8_t  s_error         = 0U;
static uint8_t  s_crcErrorCount = 0U;

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

static void delayus(uint32_t us) {
    __HAL_TIM_SET_COUNTER(&htim4, 0);
    while (__HAL_TIM_GET_COUNTER(&htim4) < us);
}

// CRC-8 (Dallas/Maxim, poly 0x31 reflected as 0x8C)
static uint8_t crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0U;
    for (uint8_t i = 0U; i < len; i++) {
        uint8_t byte = data[i];
        for (uint8_t j = 0U; j < 8U; j++) {
            uint8_t mix = (crc ^ byte) & 0x01U;
            crc >>= 1U;
            if (mix) crc ^= 0x8CU;
            byte >>= 1U;
        }
    }
    return crc;
}

// Returns 1 if device acknowledged (presence pulse detected), 0 otherwise.
// Timing per DS18B20 datasheet:
//   tRSTL  >= 480 µs  (master holds low)
//   tPDHIGH  15~60 µs (device waits before asserting presence)
//   tPDLOW   60~240 µs (device presence pulse width)
// Sampling at 70 µs after release sits in the guaranteed overlap window.
static uint8_t ds18b20Reset(void) {
    BUS_LOW();
    delayus(500U);          // Reset pulse
    BUS_HIGH();
    delayus(70U);           // Wait for presence pulse
    uint8_t present = (BUS_READ() == GPIO_PIN_RESET) ? 1U : 0U;
    delayus(430U);          // Complete the reset time slot (~500 µs after release)
    return present;
}

static void ds18b20WriteByte(uint8_t byte) {
    for (uint8_t i = 0U; i < 8U; i++) {
        if (byte & 0x01U) {
            BUS_LOW();
            delayus(10U);   // Write-1: hold low 1~15 µs
            BUS_HIGH();
            delayus(55U);   // Recovery
        } else {
            BUS_LOW();
            delayus(65U);   // Write-0: hold low 60~120 µs
            BUS_HIGH();
            delayus(5U);    // Recovery
        }
        byte >>= 1U;
    }
}

static uint8_t ds18b20ReadByte(void) {
    uint8_t byte = 0U;
    for (uint8_t i = 0U; i < 8U; i++) {
        BUS_LOW();
        delayus(2U);        // Initiate read slot (1~15 µs)
        BUS_HIGH();
        delayus(10U);       // Sample point must be within 15 µs of slot start
        if (BUS_READ() == GPIO_PIN_SET) {
            byte |= (uint8_t)(0x01U << i);
        }
        delayus(55U);       // Complete the 70 µs time slot
    }
    return byte;
}

// Issue Convert T and record the start timestamp.
// Returns 1 on success, 0 if no presence pulse.
static uint8_t ds18b20StartConversion(void) {
    if (!ds18b20Reset()) return 0U;
    ds18b20WriteByte(CMD_SKIP_ROM);
    ds18b20WriteByte(CMD_CONVERT_T);
    s_convStartTime = HAL_GetTick();
    return 1U;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ds18b20Init(void) {
    HAL_TIM_Base_Start(&htim4);

    if (!ds18b20Reset()) {
        s_error = 1U;
        return;
    }

    // Configure resolution (TH/TL alarm registers are unused; set to 0)
    ds18b20WriteByte(CMD_SKIP_ROM);
    ds18b20WriteByte(CMD_WRITE_SCRATCHPAD);
    ds18b20WriteByte(0x00U);              // TH
    ds18b20WriteByte(0x00U);              // TL
    ds18b20WriteByte(DS18B20_RESOLUTION); // Config register

    // Start the first conversion; ds18b20Read() returns 0.0f until it completes
    if (!ds18b20StartConversion()) {
        s_error = 1U;
        return;
    }

    s_error         = 0U;
    s_crcErrorCount = 0U;
}

float ds18b20Read(void) {
    if (s_error) {
        return DS18B20_ERROR_VALUE;
    }

    // Conversion still in progress — return the cached value (0.0f on first call)
    if (HAL_GetTick() - s_convStartTime < DS18B20_CONVERT_TIME_MS) {
        return s_temperature;
    }

    // ---- Conversion complete: read scratchpad ----
    uint8_t scratchpad[9];

    if (!ds18b20Reset()) {
        s_error = 1U;
        return DS18B20_ERROR_VALUE;
    }
    ds18b20WriteByte(CMD_SKIP_ROM);
    ds18b20WriteByte(CMD_READ_SCRATCHPAD);
    for (uint8_t i = 0U; i < 9U; i++) {
        scratchpad[i] = ds18b20ReadByte();
    }

    // CRC check (bytes 0-7, expected CRC in byte 8)
    if (crc8(scratchpad, 8U) != scratchpad[8]) {
        s_crcErrorCount++;
        if (s_crcErrorCount >= DS18B20_MAX_CRC_ERRORS) {
            s_error = 1U;
            return DS18B20_ERROR_VALUE;
        }
        // Transient error: restart conversion and return last known good value
        ds18b20StartConversion();
        return s_temperature;
    }
    s_crcErrorCount = 0U;

    // Parse temperature: 16-bit signed two's complement, 1 LSB = 0.0625 °C
    int16_t raw = (int16_t)((uint16_t)(scratchpad[1] << 8U) | scratchpad[0]);
    s_temperature = (float)raw * 0.0625f;

    // Immediately trigger next conversion
    if (!ds18b20StartConversion()) {
        s_error = 1U;
        return DS18B20_ERROR_VALUE;
    }

    return s_temperature;
}
