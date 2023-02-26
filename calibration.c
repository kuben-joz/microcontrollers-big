#include <gpio.h>
#include <stm32.h>
#include <string.h>
#include <irq.h>
#include "calibration.h"

int calibration_init() {
    // enable RCC->BDCR and writing to backup registers
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_DBP;
    __NOP();
    __NOP();

    // make sure tamper detection is disabled
    // doesnt seem like TAMP2E does anything, in the docs its conencted to
    // a reserved register
    RTC->TAFCR &= ~RTC_TAFCR_TAMP1E;
    // clear tamp detection if already set for some reason
    // again TAMP2F is listed as reserved in the docs
    RTC->ISR &= ~RTC_ISR_TAMP1F;
    return 0;
}


// coefficents always order 2 polynomial
int calibration_store_polynomial(float *coeff) {
    uint32_t* coeff_cp = (uint32_t*) coeff;
    RTC->BKP0R = coeff_cp[0];
    RTC->BKP1R = coeff_cp[1];
    RTC->BKP2R = coeff_cp[2];
    return 0;
}

int calibration_retrieve_polynomial(float *coeff) {
    uint32_t* coeff_cp = (uint32_t*) coeff;
    coeff_cp[0] = RTC->BKP0R; 
    coeff_cp[1] = RTC->BKP1R; 
    coeff_cp[2] = RTC->BKP2R; 
    return 0;
}