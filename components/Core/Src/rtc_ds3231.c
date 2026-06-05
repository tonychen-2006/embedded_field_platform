#include "rtc_ds3231.h"

#define DS3231_ADDR (0x68 << 1)
#define DS3231_TIME_REG 0x00
#define DS3231_REG_ADDR_SIZE 1U

static uint8_t bcd_to_dec(uint8_t value) {

    return ((value >> 4) * 10) + (value & 0x0F);
}

static uint8_t dec_to_bcd(uint8_t value) {

    return (uint8_t)(((value / 10) << 4) | (value % 10));
}

HAL_StatusTypeDef DS3231_ReadTime(I2C_HandleTypeDef *hi2c, RTC_Time *time) {

    uint8_t register_address = DS3231_TIME_REG;
    uint8_t data[7];

    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(hi2c, DS3231_ADDR, &register_address, DS3231_REG_ADDR_SIZE, HAL_MAX_DELAY);

    if (status != HAL_OK) return status;

    status = HAL_I2C_Master_Receive(hi2c, DS3231_ADDR, data, sizeof(data), HAL_MAX_DELAY);

    if (status != HAL_OK) return status;

    time->seconds = bcd_to_dec(data[0] & 0x7F);
    time->minutes = bcd_to_dec(data[1]);
    time->hours = bcd_to_dec(data[2] & 0x3F);
    time->day = bcd_to_dec(data[3]);
    time->date = bcd_to_dec(data[4]);
    time->month = bcd_to_dec(data[5] & 0x1F);
    time->year = bcd_to_dec(data[6]);

    return HAL_OK;
}

HAL_StatusTypeDef DS3231_SetTime(I2C_HandleTypeDef *hi2c, const RTC_Time *time) {

    uint8_t data[8];

    if (time == NULL) {
        return HAL_ERROR;
    }

    data[0] = DS3231_TIME_REG;
    data[1] = dec_to_bcd(time->seconds);
    data[2] = dec_to_bcd(time->minutes);
    data[3] = dec_to_bcd(time->hours);
    data[4] = dec_to_bcd(time->day);
    data[5] = dec_to_bcd(time->date);
    data[6] = dec_to_bcd(time->month);
    data[7] = dec_to_bcd(time->year);

    return HAL_I2C_Master_Transmit(
        hi2c,
        DS3231_ADDR,
        data,
        sizeof(data),
        HAL_MAX_DELAY);
}