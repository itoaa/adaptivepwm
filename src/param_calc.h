/**
 * @file param_calc.h
 * @brief Electrical parameter calculation algorithms
 * 
 * Calculates L, C, ESR from measured voltage/current waveforms.
 * These are the real implementations of the theoretical formulas.
 */

#ifndef PARAM_CALC_H
#define PARAM_CALC_H

#include <stdint.h>
#include <stdbool.h>
#include "hal_adc.h"

// Measurement buffers for ripple analysis
#define RIPPLE_BUFFER_SIZE  256
#define RIPPLE_SAMPLE_COUNT 64

typedef struct {
    float vin_samples[RIPPLE_BUFFER_SIZE];
    float vout_samples[RIPPLE_BUFFER_SIZE];
    float current_samples[RIPPLE_BUFFER_SIZE];
    uint32_t timestamps[RIPPLE_BUFFER_SIZE];
    uint16_t write_index;
    bool buffer_full;
} WaveformBuffer_t;

typedef struct {
    float inductance_mH;      // Calculated inductance
    float capacitance_uF;     // Calculated capacitance
    float esr_mOhm;          // Calculated ESR
    float ripple_current;     // Current ripple amplitude
    float ripple_voltage;     // Voltage ripple amplitude
    float switching_freq;     // Measured switching frequency
    bool valid;              // Calculations valid
    uint32_t calc_time_ms;   // Calculation timestamp
} CalculatedParams_t;

/**
 * @brief Initialize parameter calculation
 * @param buffer Waveform buffer
 * @return true if successful
 */
bool ParamCalc_Init(WaveformBuffer_t* buffer);

/**
 * @brief Add sample to waveform buffer
 * @param buffer Waveform buffer
 * @param adc_meas ADC measurement
 * @return true if buffer has enough data
 */
bool ParamCalc_AddSample(WaveformBuffer_t* buffer, const ADC_Measurement_t* adc_meas);

/**
 * @brief Calculate inductance from current ripple
 * 
 * Formula: L = (Vin - Vout) × ton / ΔI
 * Where:
 *   ton = duty_cycle / fsw
 *   ΔI = peak-to-peak current ripple
 * 
 * @param buffer Waveform buffer
 * @param duty_cycle Current duty cycle (0-1)
 * @param fsw Switching frequency (Hz)
 * @return Inductance in mH, 0 if error
 */
float ParamCalc_CalculateL(const WaveformBuffer_t* buffer, float duty_cycle, float fsw);

/**
 * @brief Calculate capacitance from voltage ripple
 * 
 * Formula: C = ΔI × D / (fsw × ΔV)
 * Where:
 *   D = duty_cycle
 *   ΔI = current ripple
 *   ΔV = voltage ripple
 * 
 * @param buffer Waveform buffer
 * @param duty_cycle Current duty cycle
 * @param fsw Switching frequency (Hz)
 * @return Capacitance in µF, 0 if error
 */
float ParamCalc_CalculateC(const WaveformBuffer_t* buffer, float duty_cycle, float fsw);

/**
 * @brief Calculate ESR from step response
 * 
 * ESR = ΔV / ΔI during load step
 * Or from AC impedance at switching frequency
 * 
 * @param buffer Waveform buffer
 * @return ESR in mΩ, 0 if error
 */
float ParamCalc_CalculateESR(const WaveformBuffer_t* buffer);

/**
 * @brief Calculate all parameters at once
 * @param buffer Waveform buffer
 * @param duty_cycle Current duty cycle
 * @param fsw Switching frequency
 * @param params Output structure
 * @return true if all calculations successful
 */
bool ParamCalc_CalculateAll(const WaveformBuffer_t* buffer, 
                            float duty_cycle, 
                            float fsw,
                            CalculatedParams_t* params);

/**
 * @brief Detect switching frequency from ripple
 * @param buffer Waveform buffer
 * @return Detected frequency in Hz
 */
float ParamCalc_DetectFrequency(const WaveformBuffer_t* buffer);

/**
 * @brief Calculate current ripple amplitude
 * @param buffer Waveform buffer
 * @return Peak-to-peak ripple current (A)
 */
float ParamCalc_CalcRippleCurrent(const WaveformBuffer_t* buffer);

/**
 * @brief Calculate voltage ripple amplitude
 * @param buffer Waveform buffer
 * @return Peak-to-peak ripple voltage (V)
 */
float ParamCalc_CalcRippleVoltage(const WaveformBuffer_t* buffer);

/**
 * @brief Validate calculated parameters
 * @param params Calculated parameters
 * @return true if within reasonable limits
 */
bool ParamCalc_Validate(const CalculatedParams_t* params);

/**
 * @brief Reset waveform buffer
 * @param buffer Waveform buffer
 */
void ParamCalc_ResetBuffer(WaveformBuffer_t* buffer);

#endif // PARAM_CALC_H
