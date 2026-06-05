#include "battery_read.h"

#include "main.h"

float read_battery_voltage(ADC_HandleTypeDef* hadc) {

    HAL_ADC_Start(hadc);

    if (HAL_ADC_PollForConversion(hadc, HAL_MAX_DELAY)) {
        return -1.0f;
    }

    uint32_t raw_adc_value = HAL_ADC_GetValue(hadc);

    float adc_voltage = ((float)raw_adc_value * 3.3f) / 4095.0f;
    float battery_voltage = adc_voltage * 2.0;

    return battery_voltage;
}