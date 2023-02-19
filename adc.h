#pragma once

#include <stm32.h>

int adc_run(uint16_t* data);

uint16_t adc_calculate_temp(uint16_t val);