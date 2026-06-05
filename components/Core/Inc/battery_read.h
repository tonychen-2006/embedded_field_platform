#ifndef BATTERY_READ_H
#define BATTERY_READ_H

#include "main.h"

float read_battery_voltage(ADC_HandleTypeDef* hadc);

#endif /* BATTERY_READ_H */