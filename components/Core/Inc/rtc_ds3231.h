#ifndef RTC_DS3231_H
#define RTC_DS3231_H

#include <stdint.h>
#include "main.h"

typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t date;
    uint8_t month;
    uint8_t year;
} RTC_Time;

HAL_StatusTypeDef DS3231_ReadTime(I2C_HandleTypeDef *hi2c, RTC_Time *time);
HAL_StatusTypeDef DS3231_SetTime(I2C_HandleTypeDef *hi2c, const RTC_Time *time);

#endif /* RTC_DS3231_H */