/**
 * @file efficiency_calc.h
 * @brief Efficiency calculation module with measurement-based validation
 * 
 * This module implements a hybrid approach to efficiency calculation:
 * - Primary: Direct power measurement (Pin vs Pout)
 * - Secondary: Physics-based loss model for diagnostics
 * 
 * The measurement-based approach is more accurate than theoretical
 * calculations because it captures all real-world losses automatically.
 * 
 * @version 1.0.0
 * @date 2026-04-16
 * @note PWM-ARCH-005: Replaces simplified placeholder formulas with validated model
 */

#ifndef EFFICIENCY_CALC_H
#define EFFICIENCY_CALC_H

#include <stdint.h>
#include <stdbool.h>
#include "hal_adc.h"

// Efficiency calculation modes
typedef enum {
    EFF_MODE_MEASUREMENT,    // Direct power measurement (recommended)
    EFF_MODE_MODEL,         // Physics-based loss model
    EFF_MODE_HYBRID         // Combined approach with validation
} EfficiencyCalcMode_t;

// Converter topology types
typedef enum {
    TOPOLOGY_BUCK,          // Step-down converter
    TOPOLOGY_BOOST,         // Step-up converter
    TOPOLOGY_BUCK_BOOST     // Inverting buck-boost
} ConverterTopology_t;

// Power measurement structure
typedef struct {
    float vin;              // Input voltage (V)
    float vout;             // Output voltage (V)
    float iin;              // Input current (A)
    float iout;             // Output current (A)
    float pin;              // Input power (W)
    float pout;             // Output power (W)
    float efficiency;       // Calculated efficiency (0-1)
    float loss_watts;       // Power loss (W)
    bool valid;             // Measurement valid flag
    uint32_t timestamp_ms;  // Measurement timestamp
} PowerMeasurement_t;

// Loss model parameters for physics-based calculation
typedef struct {
    float rds_on_high;      // High-side MOSFET on-resistance (ohm)
    float rds_on_low;       // Low-side MOSFET on-resistance (ohm)
    float gate_charge;      // Total gate charge (nC)
    float vgate;            // Gate drive voltage (V)
    float inductor_dcr;     // Inductor DC resistance (ohm)
    float core_loss_k;      // Core loss coefficient (Steinmetz)
    float core_loss_alpha;  // Core loss frequency exponent
    float core_loss_beta;   // Core loss flux density exponent
    float cap_esr;          // Output capacitor ESR (ohm)
    float switching_freq;   // Switching frequency (Hz)
} LossModelParams_t;

// Efficiency calculation context
typedef struct {
    EfficiencyCalcMode_t mode;
    ConverterTopology_t topology;
    LossModelParams_t loss_model;
    
    // Moving average filter for stability
    float efficiency_history[8];
    uint8_t history_index;
    uint8_t history_count;
    
    // Validation state
    float last_measured_eff;
    float last_calculated_eff;
    float model_error_percent;  // Difference between measured and calculated
    bool model_valid;           // Model validated against measurements
} EfficiencyCalcContext_t;

// =============================================================================
// INITIALIZATION AND CONFIGURATION
// =============================================================================

/**
 * @brief Initialize efficiency calculation module
 * @param ctx Context structure to initialize
 * @param mode Calculation mode (MEASUREMENT recommended)
 * @param topology Converter topology
 * @return true if successful
 */
bool EfficiencyCalc_Init(EfficiencyCalcContext_t* ctx,
                         EfficiencyCalcMode_t mode,
                         ConverterTopology_t topology);

/**
 * @brief Configure loss model parameters (for MODEL and HYBRID modes)
 * @param ctx Context structure
 * @param params Loss model parameters
 */
void EfficiencyCalc_SetLossModel(EfficiencyCalcContext_t* ctx,
                                  const LossModelParams_t* params);

/**
 * @brief Set calculation mode
 * @param ctx Context structure
 * @param mode New calculation mode
 */
void EfficiencyCalc_SetMode(EfficiencyCalcContext_t* ctx,
                             EfficiencyCalcMode_t mode);

// =============================================================================
// MEASUREMENT-BASED CALCULATION (PRIMARY METHOD)
// =============================================================================

/**
 * @brief Calculate efficiency from direct power measurements
 * 
 * This is the primary and most accurate method. It measures actual
 * input and output power to determine efficiency.
 * 
 * Formula: η = Pout / Pin = (Vout × Iout) / (Vin × Iin)
 * 
 * Requirements:
 * - ADC must measure Vin, Vout, and Iout
 * - Iin must be measured or estimated from power balance
 * 
 * @param ctx Context structure
 * @param meas ADC measurement structure
 * @param pm Output power measurement structure
 * @return Calculated efficiency (0-1), 0.0 if invalid
 */
float EfficiencyCalc_FromMeasurements(EfficiencyCalcContext_t* ctx,
                                      const ADC_Measurement_t* meas,
                                      PowerMeasurement_t* pm);

/**
 * @brief Estimate input current from output measurements
 * 
 * When input current sensing is not available, estimate based on
 * output current and estimated efficiency.
 * 
 * @param vout Output voltage (V)
 * @param iout Output current (A)
 * @param vin Input voltage (V)
 * @param estimated_eff Estimated efficiency (default 0.9)
 * @return Estimated input current (A)
 */
float EfficiencyCalc_EstimateInputCurrent(float vout, float iout,
                                           float vin, float estimated_eff);

// =============================================================================
// PHYSICS-BASED MODEL (DIAGNOSTIC METHOD)
// =============================================================================

/**
 * @brief Calculate efficiency using physics-based loss model
 * 
 * This secondary method breaks down losses by component for diagnostic
 * purposes. Less accurate than direct measurement but useful for
 * identifying loss sources.
 * 
 * @param ctx Context structure
 * @param meas ADC measurement structure
 * @param duty_cycle Current duty cycle (0-1)
 * @param iripple Peak-to-peak inductor ripple current (A)
 * @return Calculated efficiency (0-1), 0.0 if invalid
 */
float EfficiencyCalc_FromModel(EfficiencyCalcContext_t* ctx,
                               const ADC_Measurement_t* meas,
                               float duty_cycle,
                               float iripple);

/**
 * @brief Calculate individual loss components
 * 
 * Populates the loss breakdown structure for detailed analysis.
 * 
 * @param ctx Context structure
 * @param meas ADC measurement structure
 * @param duty_cycle Current duty cycle (0-1)
 * @param iripple Peak-to-peak inductor ripple current (A)
 * @param losses Output loss breakdown (W)
 * @param num_losses Number of loss components
 * @return Total calculated losses (W)
 */
float EfficiencyCalc_GetLossBreakdown(EfficiencyCalcContext_t* ctx,
                                       const ADC_Measurement_t* meas,
                                       float duty_cycle,
                                       float iripple,
                                       float* losses,
                                       uint8_t num_losses);

// =============================================================================
// FILTERING AND VALIDATION
// =============================================================================

/**
 * @brief Get filtered efficiency with moving average
 * @param ctx Context structure
 * @param raw_efficiency Raw efficiency value (0-1)
 * @return Filtered efficiency (0-1)
 */
float EfficiencyCalc_GetFiltered(EfficiencyCalcContext_t* ctx,
                                  float raw_efficiency);

/**
 * @brief Validate efficiency measurement
 * 
 * Checks if calculated efficiency is within reasonable bounds
 * and consistent with physical constraints.
 * 
 * @param efficiency Calculated efficiency (0-1)
 * @param pm Power measurement structure
 * @return true if efficiency is valid
 */
bool EfficiencyCalc_IsValid(float efficiency, const PowerMeasurement_t* pm);

/**
 * @brief Get model validation status
 * 
 * For HYBRID mode, returns whether the physics model has been
 * validated against measurements.
 * 
 * @param ctx Context structure
 * @return true if model is validated
 */
bool EfficiencyCalc_IsModelValidated(const EfficiencyCalcContext_t* ctx);

/**
 * @brief Get last model error percentage
 * @param ctx Context structure
 * @return Model error as percentage
 */
float EfficiencyCalc_GetModelError(const EfficiencyCalcContext_t* ctx);

// =============================================================================
// CONSTANTS AND LIMITS
// =============================================================================

// Efficiency limits for validation
#define EFF_MIN_REASONABLE      0.50f   // Minimum reasonable efficiency
#define EFF_MAX_REASONABLE      0.99f   // Maximum reasonable efficiency
#define EFF_DEFAULT_ESTIMATE    0.90f   // Default efficiency estimate

// Power limits for validation
#define POWER_MIN_MEASURABLE    0.1f    // Minimum measurable power (W)
#define POWER_MAX_REASONABLE    500.0f  // Maximum reasonable power (W)

// Loss model indices for GetLossBreakdown
#define LOSS_CONDUCTION_HS      0       // High-side conduction loss
#define LOSS_CONDUCTION_LS      1       // Low-side conduction loss
#define LOSS_SWITCHING          2       // Switching loss
#define LOSS_GATE_DRIVE         3       // Gate drive loss
#define LOSS_INDUCTOR_DCR       4       // Inductor DC resistance
#define LOSS_INDUCTOR_CORE      5       // Inductor core loss
#define LOSS_CAPACITOR_ESR      6       // Capacitor ESR loss
#define LOSS_OTHER              7       // Other/misc losses
#define LOSS_COMPONENT_COUNT    8

#endif // EFFICIENCY_CALC_H