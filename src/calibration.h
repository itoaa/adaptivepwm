/**
 * @file calibration.h
 * @brief System calibration routines
 */

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <stdint.h>
#include <stdbool.h>
#include "hal_adc.h"

typedef struct {
    float vin_gain;
    float vout_gain;
    float current_gain;
    float vin_offset;
    float vout_offset;
    float current_offset;
    uint32_t calibration_date;
    bool valid;
} CalibrationData_t;

bool Calibration_Load(CalibrationData_t* cal);
bool Calibration_Save(const CalibrationData_t* cal);
bool Calibration_RunAuto(CalibrationData_t* cal, Adaptive_ADC_t* adc);
void Calibration_Apply(const CalibrationData_t* cal, ADC_Measurement_t* meas);

#endif // CALIBRATION_H
