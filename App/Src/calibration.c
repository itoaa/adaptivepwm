/**
 * @file calibration.c
 * @brief Calibration implementation
 */

#include "calibration.h"
#include <string.h>
#include <stdio.h>

// Store in last flash pages
#define CALIBRATION_ADDR    0x0800FC00  // Last 1KB of flash

bool Calibration_Load(CalibrationData_t* cal)
{
    if (cal == NULL) return false;
    
    memcpy(cal, (void*)CALIBRATION_ADDR, sizeof(CalibrationData_t));
    
    return cal->valid;
}

bool Calibration_Save(const CalibrationData_t* cal)
{
    if (cal == NULL) return false;
    
    // Flash write implementation needed
    // Simplified: would erase and write sector
    
    return true;
}

bool Calibration_RunAuto(CalibrationData_t* cal, Adaptive_ADC_t* adc)
{
    if (cal == NULL || adc == NULL) return false;
    
    memset(cal, 0, sizeof(CalibrationData_t));
    
    // Collect samples with zero input
    // Then with known voltage
    // Calculate gain and offset
    
    cal->valid = true;
    cal->calibration_date = 0;  // RTC timestamp
    
    return true;
}

void Calibration_Apply(const CalibrationData_t* cal, ADC_Measurement_t* meas)
{
    if (cal == NULL || meas == NULL || !cal->valid) return;
    
    meas->vin = (meas->vin - cal->vin_offset) * cal->vin_gain;
    meas->vout = (meas->vout - cal->vout_offset) * cal->vout_gain;
    meas->current = (meas->current - cal->current_offset) * cal->current_gain;
}
