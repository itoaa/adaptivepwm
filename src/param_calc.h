/**
 * @file param_calc.h
 * @brief Electrical parameter calculation algorithms
 * 
 * Calculates inductance (L), capacitance (C), and ESR from measured
 * voltage/current waveforms using RMS-based algorithms.
 *
 * PWM-ARCH-003: Added DCM (Discontinuous Conduction Mode) detection
 * and improved conduction mode analysis.
 *
 * Calculation Theory:
 * ===================
 * 
 * Inductance Calculation:
 * -----------------------
 * For a buck converter in CCM:
 *   L = (Vin - Vout) × D / (fsw × ΔI)
 * 
 * Where:
 *   D = duty_cycle (ton / T)
 *   fsw = switching frequency (Hz)
 *   ΔI = peak-to-peak current ripple (A)
 *
 * In DCM (Discontinuous Conduction Mode):
 *   L = 2 × (Vin - Vout) × D / (fsw × I_peak)
 * 
 * Capacitance Calculation:
 * ------------------------
 * For a buck converter in CCM:
 *   C = ΔI × D / (8 × fsw × ΔV)
 * 
 * Where:
 *   ΔV = peak-to-peak output voltage ripple (V)
 *
 * In DCM, the formula includes additional factor.
 *
 * ESR Calculation:
 * ----------------
 *   ESR ≈ ΔV_ESR / ΔI
 * 
 * Where ΔV_ESR is the component of voltage ripple due to ESR.
 *
 * Architecture:
 * - See docs/architecture/data-flow.md for calculation flow
 * - See docs/architecture/module-deps.md for dependencies
 *
 * @version 2.3.0
 * @date 2026-04-16
 */

#ifndef PARAM_CALC_H
#define PARAM_CALC_H

#include <stdint.h>
#include <stdbool.h>
#include "hal_adc.h"

// Measurement buffers for ripple analysis
#define RIPPLE_BUFFER_SIZE  256   // Circular buffer size
#define RIPPLE_SAMPLE_COUNT 64    // Minimum samples for calculation

// PWM-ARCH-003: DCM detection threshold (current near zero)
#define DCM_CURRENT_THRESHOLD_A    0.01f   // 10mA threshold for DCM detection
#define DCM_MIN_SAMPLES_RATIO      0.1f    // At least 10% of samples near zero

/**
 * @brief Waveform sample buffer
 * 
 * Circular buffer for storing ADC samples over time.
 * Used for RMS calculations and ripple analysis.
 *
 * @callgraph
 * @callergraph
 */
typedef struct {
    float vin_samples[RIPPLE_BUFFER_SIZE];     // Input voltage samples
    float vout_samples[RIPPLE_BUFFER_SIZE];    // Output voltage samples
    float current_samples[RIPPLE_BUFFER_SIZE]; // Current samples
    uint32_t timestamps[RIPPLE_BUFFER_SIZE];   // Sample timestamps (ms)
    uint16_t write_index;                      // Current write position
    bool buffer_full;                          // True after buffer wraps
} WaveformBuffer_t;

/**
 * @brief Calculated electrical parameters
 * 
 * Results from parameter estimation calculations.
 * All fields populated by ParamCalc_CalculateAll().
 */
typedef struct {
    /* Mean operating point (always updated when buffer has enough samples) */
    float avg_vin;            // Average input voltage (V)
    float avg_vout;           // Average output voltage (V)
    float avg_current;        // Average current (A)
    bool averages_valid;      // True when mean values are usable

    /* Switch-ripple L/C/ESR — only valid if FEATURE_SWITCH_RIPPLE_ESTIMATION=1
     * and sampling is adequate for fsw (see MATURITY.md). */
    float inductance_mH;      // Calculated inductance (mH)
    float capacitance_uF;     // Calculated capacitance (µF)
    float esr_mOhm;           // Calculated ESR (mΩ)
    float ripple_current;     // Peak-to-peak ripple current (A)
    float ripple_voltage;     // Peak-to-peak ripple voltage (V)
    float switching_freq;     // Measured switching frequency (Hz)
    bool dcm_detected;        // True if DCM detected
    bool valid;               // True if L/C/ESR passed validation (ripple path)
    uint32_t calc_time_ms;    // Timestamp of calculation
} CalculatedParams_t;

/**
 * @brief Initialize parameter calculation
 * 
 * Initializes waveform buffer to empty state.
 *
 * Called by:
 * - main() during system initialization
 *
 * @callgraph
 * @callergraph
 * 
 * @param buffer Waveform buffer to initialize
 * @return true if successful
 * @return false if buffer is NULL
 */
bool ParamCalc_Init(WaveformBuffer_t* buffer);

/**
 * @brief Add sample to waveform buffer
 * 
 * Adds ADC measurement to circular buffer.
 * Thread-safe when called from single producer.
 *
 * Called by:
 * - Task_Measurement @ 1kHz
 *
 * @callgraph
 * @callergraph
 * 
 * @param buffer Waveform buffer
 * @param adc_meas ADC measurement from HAL_ADC
 * @return true if buffer has enough data for calculations (buffer_full)
 * @return false if more data needed
 */
bool ParamCalc_AddSample(WaveformBuffer_t* buffer, const ADC_Measurement_t* adc_meas);

/**
 * @brief Calculate inductance from current ripple
 * 
 * Formula (CCM mode):
 *   L = (Vin - Vout) × ton / ΔI
 *   Where ton = D / fsw
 * 
 * Formula (DCM mode, PWM-ARCH-003):
 *   L = 2 × (Vin - Vout) × ton / (fsw × I_peak)
 * 
 * Called by:
 * - ParamCalc_CalculateAll()
 * - Task_Control for monitoring
 *
 * @callgraph
 * @callergraph
 * 
 * @param buffer Waveform buffer with samples
 * @param duty_cycle Current duty cycle (0.0 - 1.0)
 * @param fsw Switching frequency in Hz
 * @return Inductance in mH, 0.0 if error or insufficient data
 */
float ParamCalc_CalculateL(const WaveformBuffer_t* buffer, float duty_cycle, float fsw);

/**
 * @brief Calculate capacitance from voltage ripple
 * 
 * Formula (CCM mode):
 *   C = ΔI × D / (8 × fsw × ΔV)
 * 
 * Formula adjusted for DCM mode when detected.
 * 
 * Called by:
 * - ParamCalc_CalculateAll()
 *
 * @callgraph
 * 
 * @param buffer Waveform buffer
 * @param duty_cycle Current duty cycle
 * @param fsw Switching frequency in Hz
 * @return Capacitance in µF, 0.0 if error
 */
float ParamCalc_CalculateC(const WaveformBuffer_t* buffer, float duty_cycle, float fsw);

/**
 * @brief Calculate ESR from step response
 * 
 * ESR estimated from voltage/current phase relationship.
 * Simplified: ESR ≈ ΔV_ESR_component / ΔI
 * 
 * Called by:
 * - ParamCalc_CalculateAll()
 *
 * @callgraph
 * 
 * @param buffer Waveform buffer
 * @return ESR in mΩ, 0.0 if error
 */
float ParamCalc_CalculateESR(const WaveformBuffer_t* buffer);

/**
 * @brief Calculate all parameters at once
 * 
 * Convenience function that performs all calculations:
 * 1. Calculate ripple current and voltage
 * 2. Detect switching frequency
 * 3. Detect conduction mode (DCM/CCM)
 * 4. Calculate L, C, ESR
 * 5. Validate results
 *
 * Called by:
 * - Task_Control @ 100Hz
 *
 * Processing Flow:
 * ```
 * Get sample count
 *     ↓
 * CalcRippleCurrent() → RMS of current samples
 * CalcRippleVoltage() → RMS of voltage samples  
 * DetectFrequency() → Zero crossings
 * DetectConductionMode() → DCM/CCM
 *     ↓
 * CalculateL() → CCM or DCM formula
 * CalculateC() → CCM or DCM formula
 * CalculateESR() → ESR estimate
 *     ↓
 * ParamCalc_Validate() → Check ranges
 * ```
 *
 * @callgraph
 * @callergraph
 * 
 * @param buffer Waveform buffer
 * @param duty_cycle Current duty cycle
 * @param fsw Expected switching frequency
 * @param params Output structure (cleared on entry)
 * @return true if all calculations successful and validated
 * @return false if any calculation failed or out of range
 */
bool ParamCalc_CalculateAll(const WaveformBuffer_t* buffer, 
                            float duty_cycle, 
                            float fsw,
                            CalculatedParams_t* params);

/**
 * @brief Detect switching frequency from ripple
 * 
 * Counts zero crossings in current waveform to estimate
 * actual switching frequency.
 *
 * Called by:
 * - ParamCalc_CalculateAll()
 *
 * @callgraph
 * 
 * @param buffer Waveform buffer
 * @return Detected frequency in Hz, 0.0 if insufficient data
 */
float ParamCalc_DetectFrequency(const WaveformBuffer_t* buffer);

/**
 * @brief Calculate current ripple amplitude
 * 
 * RMS-based calculation for more accurate results than
 * simple peak-to-peak.
 * 
 * RMS conversion: I_pp = I_rms × 2 × √2
 *
 * Called by:
 * - ParamCalc_CalculateAll()
 * - Task_Safety for ripple monitoring
 *
 * @callgraph
 * @callergraph
 * 
 * @param buffer Waveform buffer
 * @return Peak-to-peak ripple current in Amps, 0.0 if error
 */
float ParamCalc_CalcRippleCurrent(const WaveformBuffer_t* buffer);

/**
 * @brief Calculate voltage ripple amplitude
 * 
 * Similar to current ripple calculation.
 *
 * Called by:
 * - ParamCalc_CalculateAll()
 *
 * @callgraph
 * 
 * @param buffer Waveform buffer
 * @return Peak-to-peak ripple voltage in Volts, 0.0 if error
 */
float ParamCalc_CalcRippleVoltage(const WaveformBuffer_t* buffer);

/**
 * @brief Validate calculated parameters
 * 
 * Checks that calculated values are within reasonable limits:
 * - L: 1µH to 100mH
 * - C: 0.1µF to 10mF
 * - ESR: 10µΩ to 1Ω
 * - Ripple: > 1mV, > 1mA
 *
 * Called by:
 * - ParamCalc_CalculateAll()
 *
 * @callgraph
 * 
 * @param params Calculated parameters
 * @return true if all values within valid ranges
 * @return false if any value out of range
 */
bool ParamCalc_Validate(const CalculatedParams_t* params);

/**
 * @brief Reset waveform buffer
 * 
 * Clears all samples. Called when changing operating points
 * or after calibration.
 *
 * @callgraph
 * @callergraph
 * 
 * @param buffer Waveform buffer
 */
void ParamCalc_ResetBuffer(WaveformBuffer_t* buffer);

/**
 * @brief Detect conduction mode (DCM vs CCM)
 * 
 * PWM-ARCH-003: Detects if converter is operating in
 * Discontinuous Conduction Mode (current touches zero during cycle)
 * or Continuous Conduction Mode.
 *
 * DCM Detection Algorithm:
 * 1. Calculate average current
 * 2. If average < threshold: likely DCM
 * 3. Count samples near zero (< DCM_CURRENT_THRESHOLD_A)
 * 4. If > DCM_MIN_SAMPLES_RATIO near zero: DCM detected
 *
 * Called by:
 * - ParamCalc_CalculateAll() (to select correct formulas)
 * - Task_Control for monitoring
 *
 * @callgraph
 * @callergraph
 * 
 * @param buffer Waveform buffer
 * @return true if DCM detected
 * @return false if CCM or insufficient data
 */
bool ParamCalc_DetectConductionMode(const WaveformBuffer_t* buffer);

/**
 * @brief Get minimum current from buffer
 * 
 * Utility function for DCM detection.
 *
 * @param buffer Waveform buffer
 * @return Minimum current value in Amps, 0.0 if no data
 */
float ParamCalc_GetMinCurrent(const WaveformBuffer_t* buffer);

/**
 * @brief Get maximum current from buffer
 * 
 * Used in DCM inductance calculation.
 *
 * @param buffer Waveform buffer
 * @return Maximum current value in Amps, 0.0 if no data
 */
float ParamCalc_GetMaxCurrent(const WaveformBuffer_t* buffer);

#endif // PARAM_CALC_H
