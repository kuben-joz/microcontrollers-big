#pragma once

#include <stm32.h>

int adc_run(uint16_t transfer_num, uint32_t dest);

float adc_calculate_temp(uint16_t val);

int adc_dma_start(void);

int adc_restart(uint16_t transfer_num, uint32_t dest);

int adc_stop();