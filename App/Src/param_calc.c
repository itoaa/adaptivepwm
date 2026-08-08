/**
 * @file param_calc.c
 * @brief Electrical parameter calculation implementation with RMS
 * 
 * PWM-ARCH-003: Added DCM (Discontinuous Conduction Mode) detection
 * and improved conduction mode analysis.
 * 
 * @version 2.3.0
 * @date 2026-04-16
 */

#include "param_calc.h"
#include "config.h"
#include "adaptive_assert.h"
#include <math.h>
#include <string.h>

// Parameter limits for validation
#define MIN_INDUCTANCE_MH    0.001f   // 1uH
#define MAX_INDUCTANCE_MH    100.0f   // 100mH
#define MIN_CAPACITANCE_UF   0.1f     // 0.1uF
#define MAX_CAPACITANCE_UF   10000.0f // 10mF
#define MIN_ESR_MOHM         0.01f    // 10uOhm
#define MAX_ESR_MOHM         1000.0f  // 1Ohm

// Minimum samples for calculation
#define MIN_SAMPLES_FOR_CALC 32

bool ParamCalc_Init(WaveformBuffer_t* buffer)
{
    ADAPTIVE_ASSERT(buffer != NULL);
    
    if (buffer == NULL) return false;
    memset(buffer, 0, sizeof(WaveformBuffer_t));
    return true;
}

bool ParamCalc_AddSample(WaveformBuffer_t* buffer, const ADC_Measurement_t* adc_meas)
{
    ADAPTIVE_ASSERT(buffer != NULL);
    ADAPTIVE_ASSERT(adc_meas != NULL);
    
    if (buffer == NULL || adc_meas == NULL) return false;
    
    buffer->vin_samples[buffer->write_index] = adc_meas->vin;
    buffer->vout_samples[buffer->write_index] = adc_meas->vout;
    buffer->current_samples[buffer->write_index] = adc_meas->current;
    buffer->timestamps[buffer->write_index] = adc_meas->timestamp;
    
    buffer->write_index++;
    if (buffer->write_index >= RIPPLE_BUFFER_SIZE) {
        buffer->write_index = 0;
        buffer->buffer_full = true;
    }
    
    return buffer->buffer_full;
}

void ParamCalc_ResetBuffer(WaveformBuffer_t* buffer)
{
    ADAPTIVE_ASSERT(buffer != NULL);
    
    if (buffer == NULL) return;
    memset(buffer, 0, sizeof(WaveformBuffer_t));
}

// Calculate RMS value from samples
static float CalcRMS(const float* samples, uint16_t count)
{
    if (count == 0) return 0.0f;
    
    float sum_sq = 0.0f;
    float avg = 0.0f;
    
    // Calculate mean
    for (uint16_t i = 0; i < count; i++) {
        avg += samples[i];
    }
    avg /= count;
    
    // Calculate RMS around mean (AC component)
    for (uint16_t i = 0; i < count; i++) {
        float diff = samples[i] - avg;
        sum_sq += diff * diff;
    }
    
    return sqrtf(sum_sq / count);
}

// PWM-ARCH-003: Helper to get sample count
static uint16_t GetSampleCount(const WaveformBuffer_t* buffer)
{
    return buffer->buffer_full ? RIPPLE_BUFFER_SIZE : buffer->write_index;
}

float ParamCalc_CalcRippleCurrent(const WaveformBuffer_t* buffer)
{
    ADAPTIVE_ASSERT(buffer != NULL);
    
    if (buffer == NULL) return 0.0f;
    
    uint16_t count = GetSampleCount(buffer);
    if (count < MIN_SAMPLES_FOR_CALC) return 0.0f;
    
    // RMS gives more accurate ripple than peak-to-peak
    return CalcRMS(buffer->current_samples, count) * 2.0f * 1.414f;  // Convert RMS to peak-to-peak
}

float ParamCalc_CalcRippleVoltage(const WaveformBuffer_t* buffer)
{
    ADAPTIVE_ASSERT(buffer != NULL);
    
    if (buffer == NULL) return 0.0f;
    
    uint16_t count = GetSampleCount(buffer);
    if (count < MIN_SAMPLES_FOR_CALC) return 0.0f;
    
    return CalcRMS(buffer->vout_samples, count) * 2.0f * 1.414f;
}

// PWM-ARCH-003: Get minimum current from buffer
float ParamCalc_GetMinCurrent(const WaveformBuffer_t* buffer)
{
    ADAPTIVE_ASSERT(buffer != NULL);
    
    if (buffer == NULL) return 0.0f;
    
    uint16_t count = GetSampleCount(buffer);
    if (count == 0) return 0.0f;
    
    float min_current = INFINITY;
    for (uint16_t i = 0; i < count; i++) {
        if (buffer->current_samples[i] < min_current) {
            min_current = buffer->current_samples[i];
        }
    }
    
    return min_current;
}

// PWM-ARCH-003: Get maximum current from buffer
float ParamCalc_GetMaxCurrent(const WaveformBuffer_t* buffer)
{
    ADAPTIVE_ASSERT(buffer != NULL);
    
    if (buffer == NULL) return 0.0f;
    
    uint16_t count = GetSampleCount(buffer);
    if (count == 0) return 0.0f;
    
    float max_current = -INFINITY;
    for (uint16_t i = 0; i < count; i++) {
        if (buffer->current_samples[i] > max_current) {
            max_current = buffer->current_samples[i];
        }
    }
    
    return max_current;
}

// PWM-ARCH-003: Detect conduction mode (DCM vs CCM)
// Returns true if DCM detected (current touches zero)
bool ParamCalc_DetectConductionMode(const WaveformBuffer_t* buffer)
{
    ADAPTIVE_ASSERT(buffer != NULL);
    
    if (buffer == NULL) return false;
    
    uint16_t count = GetSampleCount(buffer);
    if (count < MIN_SAMPLES_FOR_CALC) return false;
    
    // Get average current
    float avg_current = 0.0f;
    for (uint16_t i = 0; i < count; i++) {
        avg_current += buffer->current_samples[i];
    }
    avg_current /= count;
    
    // If average current is very low, likely in DCM
    if (avg_current < 0.05f) {
        return true;
    }
    
    // Count samples near zero (DCM indicator)
    uint16_t near_zero_count = 0;
    for (uint16_t i = 0; i < count; i++) {
        if (buffer->current_samples[i] < DCM_CURRENT_THRESHOLD_A) {
            near_zero_count++;
        }
    }
    
    // DCM if more than threshold ratio of samples near zero
    float near_zero_ratio = (float)near_zero_count / (float)count;
    
    return (near_zero_ratio > DCM_MIN_SAMPLES_RATIO);
}

float ParamCalc_CalculateL(const WaveformBuffer_t* buffer, float duty_cycle, float fsw)
{
    ADAPTIVE_ASSERT(buffer != NULL);
    ADAPTIVE_ASSERT(fsw > 0);
    ADAPTIVE_ASSERT(duty_cycle > 0 && duty_cycle < 1);
    
    if (buffer == NULL || fsw <= 0 || duty_cycle <= 0 || duty_cycle >= 1) {
        return 0.0f;
    }
    
    float ripple_current = ParamCalc_CalcRippleCurrent(buffer);
    if (ripple_current < 0.001f) return 0.0f;
    
    // Get average Vin and Vout from buffer
    float avg_vin = 0, avg_vout = 0;
    uint16_t count = GetSampleCount(buffer);
    
    for (uint16_t i = 0; i < count; i++) {
        avg_vin += buffer->vin_samples[i];
        avg_vout += buffer->vout_samples[i];
    }
    avg_vin /= count;
    avg_vout /= count;
    
    // PWM-ARCH-003: Adjust calculation for DCM if detected
    // In DCM, ripple current calculation is different
    bool dcm = ParamCalc_DetectConductionMode(buffer);
    
    // L = (Vin - Vout) * D / (fsw * ΔI)
    float voltage_diff = fabsf(avg_vin - avg_vout);
    float ton = duty_cycle / fsw;
    
    float inductance;
    if (dcm) {
        // DCM: Use modified formula
        // In DCM, ΔI = I_peak (since current starts from zero)
        // L = 2 * (Vin - Vout) * D / (fsw * ΔI_peak)
        float i_peak = ParamCalc_GetMaxCurrent(buffer);
        if (i_peak < 0.001f) return 0.0f;
        inductance = 2.0f * voltage_diff * ton / (fsw * i_peak);
    } else {
        // CCM: Standard formula
        inductance = (voltage_diff * ton) / ripple_current;
    }
    
    // Convert to mH
    return inductance * 1000.0f;
}

float ParamCalc_CalculateC(const WaveformBuffer_t* buffer, float duty_cycle, float fsw)
{
    ADAPTIVE_ASSERT(buffer != NULL);
    ADAPTIVE_ASSERT(fsw > 0);
    ADAPTIVE_ASSERT(duty_cycle > 0);
    
    if (buffer == NULL || fsw <= 0 || duty_cycle <= 0) {
        return 0.0f;
    }
    
    float ripple_current = ParamCalc_CalcRippleCurrent(buffer);
    float ripple_voltage = ParamCalc_CalcRippleVoltage(buffer);
    
    if (ripple_voltage < 0.001f || ripple_current < 0.001f) return 0.0f;
    
    // PWM-ARCH-003: Adjust for conduction mode
    bool dcm = ParamCalc_DetectConductionMode(buffer);
    float capacitance;
    
    if (dcm) {
        // DCM: C = ΔI / (fsw * ΔV) * k (where k is a factor accounting for DCM)
        // Simplified: similar formula but different constant
        capacitance = (ripple_current * duty_cycle) / (4.0f * fsw * ripple_voltage);
    } else {
        // CCM: C = ΔI / (8 * fsw * ΔV) for buck converter
        capacitance = (ripple_current * duty_cycle) / (8.0f * fsw * ripple_voltage);
    }
    
    // Convert to uF
    return capacitance * 1000000.0f;
}

float ParamCalc_CalculateESR(const WaveformBuffer_t* buffer)
{
    ADAPTIVE_ASSERT(buffer != NULL);
    
    if (buffer == NULL) return 0.0f;
    
    // ESR from voltage/current phase relationship at switching frequency
    // Simplified: ESR ≈ ΔV_ESR / ΔI
    
    float ripple_current = ParamCalc_CalcRippleCurrent(buffer);
    float ripple_voltage = ParamCalc_CalcRippleVoltage(buffer);
    
    if (ripple_current < 0.001f) return 0.0f;
    
    // PWM-ARCH-003: Adjust ESR estimate based on conduction mode
    // In DCM, the ESR contribution is more pronounced
    bool dcm = ParamCalc_DetectConductionMode(buffer);
    float esr_contribution = dcm ? 0.5f : 0.3f;  // Higher contribution in DCM
    
    // Estimate ESR component of voltage ripple
    // Capacitive reactance reduces at higher frequencies
    float esr_voltage = ripple_voltage * esr_contribution;
    
    float esr = esr_voltage / ripple_current;
    
    // Convert to mOhm
    return esr * 1000.0f;
}

float ParamCalc_DetectFrequency(const WaveformBuffer_t* buffer)
{
    ADAPTIVE_ASSERT(buffer != NULL);
    
    if (buffer == NULL) return 0.0f;
    
    uint16_t count = GetSampleCount(buffer);
    if (count < 4) return 0.0f;
    
    // Count zero crossings in current waveform
    uint16_t crossings = 0;
    float avg_current = 0;
    
    for (uint16_t i = 0; i < count; i++) {
        avg_current += buffer->current_samples[i];
    }
    avg_current /= count;
    
    for (uint16_t i = 1; i < count; i++) {
        if ((buffer->current_samples[i-1] < avg_current && 
             buffer->current_samples[i] >= avg_current) ||
            (buffer->current_samples[i-1] > avg_current && 
             buffer->current_samples[i] <= avg_current)) {
            crossings++;
        }
    }
    
    if (crossings < 2) return 0.0f;
    
    // Time span
    uint32_t time_ms = buffer->timestamps[count - 1] - buffer->timestamps[0];
    if (time_ms == 0) return 0.0f;
    
    // Frequency = crossings / (2 * time)
    return (crossings / 2.0f) / (time_ms / 1000.0f);
}

bool ParamCalc_CalculateAll(const WaveformBuffer_t* buffer, 
                           float duty_cycle, 
                           float fsw,
                           CalculatedParams_t* params)
{
    ADAPTIVE_ASSERT(buffer != NULL);
    ADAPTIVE_ASSERT(params != NULL);
    
    if (buffer == NULL || params == NULL) return false;
    
    memset(params, 0, sizeof(CalculatedParams_t));
    
    uint16_t count = GetSampleCount(buffer);
    if (count < MIN_SAMPLES_FOR_CALC) return false;

    /* Always compute mean operating point (control path does not need L/C) */
    float sum_vin = 0.0f, sum_vout = 0.0f, sum_i = 0.0f;
    for (uint16_t i = 0; i < count; i++) {
        sum_vin += buffer->vin_samples[i];
        sum_vout += buffer->vout_samples[i];
        sum_i += buffer->current_samples[i];
    }
    params->avg_vin = sum_vin / (float)count;
    params->avg_vout = sum_vout / (float)count;
    params->avg_current = sum_i / (float)count;
    params->averages_valid = true;
    params->calc_time_ms = HAL_GetTick();

#if FEATURE_SWITCH_RIPPLE_ESTIMATION
    /*
     * Experimental: estimate L/C/ESR from ripple. Valid only if ADC capture
     * rate is well above PWM_FREQUENCY_HZ (Nyquist). Default host path is
     * ~1 kHz into this buffer with 20 kHz PWM — results will be wrong.
     */
    params->ripple_current = ParamCalc_CalcRippleCurrent(buffer);
    params->ripple_voltage = ParamCalc_CalcRippleVoltage(buffer);
    params->switching_freq = ParamCalc_DetectFrequency(buffer);
    params->dcm_detected = ParamCalc_DetectConductionMode(buffer);
    params->inductance_mH = ParamCalc_CalculateL(buffer, duty_cycle, fsw);
    params->capacitance_uF = ParamCalc_CalculateC(buffer, duty_cycle, fsw);
    params->esr_mOhm = ParamCalc_CalculateESR(buffer);
    params->valid = ParamCalc_Validate(params);
    return params->valid;
#else
    (void)duty_cycle;
    (void)fsw;
    params->valid = false;  /* L/C/ESR intentionally not estimated */
    return params->averages_valid;
#endif
}

bool ParamCalc_Validate(const CalculatedParams_t* params)
{
    ADAPTIVE_ASSERT(params != NULL);
    
    if (params == NULL) return false;
    
    if (params->inductance_mH < MIN_INDUCTANCE_MH || 
        params->inductance_mH > MAX_INDUCTANCE_MH) return false;
    
    if (params->capacitance_uF < MIN_CAPACITANCE_UF || 
        params->capacitance_uF > MAX_CAPACITANCE_UF) return false;
    
    if (params->esr_mOhm < MIN_ESR_MOHM || 
        params->esr_mOhm > MAX_ESR_MOHM) return false;
    
    if (params->ripple_current < 0.001f) return false;
    if (params->ripple_voltage < 0.001f) return false;
    
    return true;
}
