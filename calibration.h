#pragma once

int calibration_init();

int calibration_store_polynomial(float *coeff);

int calibration_retrieve_polynomial(float *coeff);