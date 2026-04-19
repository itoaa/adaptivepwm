/**
 * @file pid_controller_optimized.h
 * @brief Optimized PID Controller with Fixed-Point Support
 * 
 * Optimizations:
 * - Optional fixed-point arithmetic for faster computation
 * - Inline functions for critical path
 * - Reduced floating-point operations
 * - Compiler hints for hot paths
 * 
 * @version 2.3.2-OPT
 * @date 2026-04-16
 */

#ifndef PID_CONTROLLER_OPTIMIZED_H
#define PID_CONTROLLER_OPTIMIZED_H

#include "config_optimized.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// FLOATING-POINT PID CONTROLLER
// =============================================================================

typedef struct {
    float Kp, Ki, Kd;
    float setpoint_weight;
    float derivative_filter;
    float integral;
    float integral_min, integral_max;
    float prev_error;
    float prev_measurement;
    float output_min, output_max;
    bool initialized;
} PID_Controller_t;

// =============================================================================
// FIXED-POINT PID CONTROLLER
// =============================================================================

#if PID_FIXED_POINT_ENABLED

typedef struct {
    int32_t Kp, Ki, Kd;          // Q16.16 format
    int32_t setpoint_weight;       // Q16.16 format (0.0 - 1.0)
    int32_t derivative_filter;     // Q16.16 format
    int64_t integral;              // Q32.32 for headroom
    int64_t integral_min, integral_max;
    int32_t prev_error;
    int32_t prev_measurement;
    int32_t output_min, output_max; // Q16.16 format
    bool initialized;
} PID_FixedPoint_t;

#endif // PID_FIXED_POINT_ENABLED

// =============================================================================
// API FUNCTIONS
// =============================================================================

/**
 * @brief Initialize PID controller
 */
void PID_Init(PID_Controller_t* pid, float Kp, float Ki, float Kd, float output_min, float output_max);

/**
 * @brief Compute PID output (optimized)
 * 
 * Uses fast paths for common operations:
 * - Inlined arithmetic
 * - Branch prediction hints
 * - Minimal branching in hot path
 */
float FAST_FUNC PID_Compute(PID_Controller_t* pid, float setpoint, float measurement, float dt);

void PID_Reset(PID_Controller_t* pid);
void PID_SetGains(PID_Controller_t* pid, float Kp, float Ki, float Kd);
void PID_SetSetpointWeight(PID_Controller_t* pid, float weight);
void PID_SetDerivativeFilter(PID_Controller_t* pid, float alpha);
float PID_GetIntegral(const PID_Controller_t* pid);
void PID_SetIntegral(PID_Controller_t* pid, float integral);

#if PID_FIXED_POINT_ENABLED

/**
 * @brief Initialize fixed-point PID controller
 */
void PID_Fixed_Init(PID_FixedPoint_t* pid, float Kp, float Ki, float Kd, float output_min, float output_max);

/**
 * @brief Compute fixed-point PID output
 * 
 * Uses pure integer arithmetic for maximum speed.
 * Approximately 5-10x faster than floating-point on Cortex-M4
 * without FPU.
 */
int32_t FAST_FUNC PID_Fixed_Compute(PID_FixedPoint_t* pid, int32_t setpoint, int32_t measurement, int32_t dt);

void PID_Fixed_Reset(PID_FixedPoint_t* pid);
void PID_Fixed_SetGains(PID_FixedPoint_t* pid, float Kp, float Ki, float Kd);
float PID_Fixed_GetOutputFloat(int32_t fixed_output);

#endif // PID_FIXED_POINT_ENABLED

// =============================================================================
// INLINE OPTIMIZED FUNCTIONS
// =============================================================================

/**
 * @brief Fast float clamp (branchless)
 */
static FORCE_INLINE float fast_clamp_float(float value, float min, float max) {
    float tmp = (value < min) ? min : value;
    return (tmp > max) ? max : tmp;
}

/**
 * @brief Fast integer clamp
 */
static FORCE_INLINE int32_t fast_clamp_int32(int32_t value, int32_t min, int32_t max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/**
 * @brief Fast float to fixed-point conversion
 */
#if PID_FIXED_POINT_ENABLED
static FORCE_INLINE int32_t float_to_fixed(float f) {
    return (int32_t)(f * PID_FIXED_POINT_SCALE);
}

/**
 * @brief Fast fixed-point to float conversion
 */
static FORCE_INLINE float fixed_to_float(int32_t fixed) {
    return ((float)fixed) / PID_FIXED_POINT_SCALE;
}

/**
 * @brief Fixed-point multiplication with saturation
 */
static FORCE_INLINE int32_t fixed_multiply(int32_t a, int32_t b) {
    int64_t result = ((int64_t)a * (int64_t)b) >> PID_FIXED_POINT_SHIFT;
    
    // Saturate on overflow
    if (result > INT32_MAX) return INT32_MAX;
    if (result < INT32_MIN) return INT32_MIN;
    return (int32_t)result;
}
#endif

#ifdef __cplusplus
}
#endif

#endif // PID_CONTROLLER_OPTIMIZED_H
