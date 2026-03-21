/**
 * @file param_calc.c
 * @brief Electrical parameter calculation implementation with RMS
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

float ParamCalc_CalcRippleCurrent(const WaveformBuffer_t* buffer)
{
    ADAPTIVE_ASSERT(buffer != NULL);
    
    if (buffer == NULL) return 0.0f;
    
    uint16_t count = buffer->buffer_full ? RIPPLE_BUFFER_SIZE : buffer->write_index;
    if (count < MIN_SAMPLES_FOR_CALC) return 0.0f;
    
    // RMS gives more accurate ripple than peak-to-peak
    return CalcRMS(buffer->current_samples, count) * 2.0f * 1.414f;  // Convert RMS to peak-to-peak
}

float ParamCalc_CalcRippleVoltage(const WaveformBuffer_t* buffer)
{
    ADAPTIVE_ASSERT(buffer != NULL);
    
    if (buffer == NULL) return 0.0f;
    
    uint16_t count = buffer->buffer_full ? RIPPLE_BUFFER_SIZE : buffer->write_index;
    if (count < MIN_SAMPLES_FOR_CALC) return 0.0f;
    
    return CalcRMS(buffer->vout_samples, count) * 2.0f * 1.414f;
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
    uint16_t count = buffer->buffer_full ? RIPPLE_BUFFER_SIZE : buffer->write_index;
    
    for (uint16_t i = 0; i < count; i++) {
        avg_vin += buffer->vin_samples[i];
        avg_vout += buffer->vout_samples[i];
    }
    avg_vin /= count;
    avg_vout /= count;
    
    // L = (Vin - Vout) * D / (fsw * ΔI)
    float voltage_diff = fabsf(avg_vin - avg_vout);
    float ton = duty_cycle / fsw;
    
    float inductance = (voltage_diff * ton) / ripple_current;
    
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
    
    // C = ΔI / (8 * fsw * ΔV) for buck converter
    // Using RMS-based calculation
    float capacitance = (ripple_current * duty_cycle) / (8.0f * fsw * ripple_voltage);
    
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
    
    // Estimate ESR component of voltage ripple
    // Capacitive reactance reduces at higher frequencies
    float esr_voltage = ripple_voltage * 0.3f;  // Estimate ESR contribution
    
    float esr = esr_voltage / ripple_current;
    
    // Convert to mOhm
    return esr * 1000.0f;
}

float ParamCalc_DetectFrequency(const WaveformBuffer_t* buffer)
{
    ADAPTIVE_ASSERT(buffer != NULL);
    
    if (buffer == NULL) return 0.0f;
    
    uint16_t count = buffer->buffer_full ? RIPPLE_BUFFER_SIZE : buffer->write_index;
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
    
    uint16_t count = buffer->buffer_full ? RIPPLE_BUFFER_SIZE : buffer->write_index;
    if (count < MIN_SAMPLES_FOR_CALC) return false;
    
    params->ripple_current = ParamCalc_CalcRippleCurrent(buffer);
    params->ripple_voltage = ParamCalc_CalcRippleVoltage(buffer);
    params->switching_freq = ParamCalc_DetectFrequency(buffer);
    
    params->inductance_mH = ParamCalc_CalculateL(buffer, duty_cycle, fsw);
    params->capacitance_uF = ParamCalc_CalculateC(buffer, duty_cycle, fsw);
    params->esr_mOhm = ParamCalc_CalculateESR(buffer);
    
    params->valid = ParamCalc_Validate(params);
    params->calc_time_ms = HAL_GetTick();
    
    return params->valid;
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
