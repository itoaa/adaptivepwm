/**
 * @file pid_controller_optimized.c
 * @brief Optimized PID Controller Implementation
 * 
 * Optimizations:
 * - Branchless operations where possible
 * - Predictable memory access patterns
 * - Reduced function call overhead
 * - Optional fixed-point arithmetic
 * 
 * @version 2.3.2-OPT
 * @date 2026-04-16
 */

#include "pid_controller_optimized.h"

// =============================================================================
// FLOATING-POINT IMPLEMENTATION
// =============================================================================

void PID_Init(PID_Controller_t* pid, float Kp, float Ki, float Kd, float output_min, float output_max)
{
    if (pid == NULL) return;
    
    pid->Kp = Kp;
    pid->Ki = Ki;
    pid->Kd = Kd;
    pid->setpoint_weight = 1.0f;
    pid->derivative_filter = 0.1f;
    
    pid->output_min = output_min;
    pid->output_max = output_max;
    
    // Conservative integral limits
    float range = output_max - output_min;
    if (Ki > 1e-6f) {
        pid->integral_max = range * 0.5f / Ki;
        pid->integral_min = -pid->integral_max;
    } else {
        pid->integral_max = 1e6f;
        pid->integral_min = -1e6f;
    }
    
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->prev_measurement = 0.0f;
    pid->initialized = false;
}

float FAST_FUNC PID_Compute(PID_Controller_t* pid, float setpoint, float measurement, float dt)
{
    if (pid == NULL || dt <= 0.0f) return 0.0f;
    
    // Error calculation
    float error = setpoint - measurement;
    
    // Proportional term with setpoint weighting
    float proportional = pid->Kp * (pid->setpoint_weight * setpoint - measurement);
    
    // Integral term with clamping
    pid->integral += error * dt;
    pid->integral = fast_clamp_float(pid->integral, pid->integral_min, pid->integral_max);
    float integral = pid->Ki * pid->integral;
    
    // Derivative term on measurement (not error) with filtering
    float derivative = 0.0f;
    if (pid->initialized) {
        float d_meas = (measurement - pid->prev_measurement) / dt;
        // Low-pass filter on derivative
        static float d_filtered = 0.0f;
        d_filtered = d_filtered * pid->derivative_filter + d_meas * (1.0f - pid->derivative_filter);
        derivative = -pid->Kd * d_filtered;
    }
    
    // Calculate output
    float output = proportional + integral + derivative;
    
    // Output saturation with back-calculation
    float saturated = fast_clamp_float(output, pid->output_min, pid->output_max);
    
    // Back-calculate integral if saturated
    if (saturated != output && pid->Ki > 1e-6f) {
        pid->integral = (saturated - proportional - derivative) / pid->Ki;
        pid->integral = fast_clamp_float(pid->integral, pid->integral_min, pid->integral_max);
    }
    
    // Store for next iteration
    pid->prev_error = error;
    pid->prev_measurement = measurement;
    pid->initialized = true;
    
    return saturated;
}

void PID_Reset(PID_Controller_t* pid)
{
    if (pid == NULL) return;
    
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->prev_measurement = 0.0f;
    pid->initialized = false;
}

void PID_SetGains(PID_Controller_t* pid, float Kp, float Ki, float Kd)
{
    if (pid == NULL) return;
    
    pid->Kp = Kp;
    pid->Ki = Ki;
    pid->Kd = Kd;
    
    // Recalculate integral limits
    float range = pid->output_max - pid->output_min;
    if (Ki > 1e-6f) {
        pid->integral_max = range * 0.5f / Ki;
        pid->integral_min = -pid->integral_max;
    }
}

void PID_SetSetpointWeight(PID_Controller_t* pid, float weight)
{
    if (pid == NULL) return;
    pid->setpoint_weight = fast_clamp_float(weight, 0.0f, 1.0f);
}

void PID_SetDerivativeFilter(PID_Controller_t* pid, float alpha)
{
    if (pid == NULL) return;
    pid->derivative_filter = fast_clamp_float(alpha, 0.0f, 1.0f);
}

float PID_GetIntegral(const PID_Controller_t* pid)
{
    if (pid == NULL) return 0.0f;
    return pid->integral;
}

void PID_SetIntegral(PID_Controller_t* pid, float integral)
{
    if (pid == NULL) return;
    pid->integral = fast_clamp_float(integral, pid->integral_min, pid->integral_max);
}

// =============================================================================
// FIXED-POINT IMPLEMENTATION
// =============================================================================

#if PID_FIXED_POINT_ENABLED

// Conversion constants
#define FIXED_ONE       PID_FIXED_POINT_SCALE
#define FIXED_HALF      (FIXED_ONE / 2)

void PID_Fixed_Init(PID_FixedPoint_t* pid, float Kp, float Ki, float Kd, float output_min, float output_max)
{
    if (pid == NULL) return;
    
    pid->Kp = float_to_fixed(Kp);
    pid->Ki = float_to_fixed(Ki);
    pid->Kd = float_to_fixed(Kd);
    pid->setpoint_weight = FIXED_ONE;  // 1.0 in fixed-point
    pid->derivative_filter = float_to_fixed(0.1f);
    
    pid->output_min = float_to_fixed(output_min);
    pid->output_max = float_to_fixed(output_max);
    
    // Calculate integral limits
    int32_t range = pid->output_max - pid->output_min;
    if (Ki > 1e-6f) {
        int32_t range_fixed = float_to_fixed((output_max - output_min) * 0.5f / Ki);
        pid->integral_max = range_fixed;
        pid->integral_min = -range_fixed;
    } else {
        pid->integral_max = (int64_t)INT32_MAX << 16;  // Large value
        pid->integral_min = -pid->integral_max;
    }
    
    pid->integral = 0;
    pid->prev_error = 0;
    pid->prev_measurement = 0;
    pid->initialized = false;
}

int32_t FAST_FUNC PID_Fixed_Compute(PID_FixedPoint_t* pid, int32_t setpoint, int32_t measurement, int32_t dt)
{
    if (pid == NULL || dt <= 0) return 0;
    
    // Error calculation (setpoint - measurement)
    int32_t error = setpoint - measurement;
    
    // Proportional term: Kp * (setpoint_weight * setpoint - measurement)
    int32_t weighted_sp = fixed_multiply(pid->setpoint_weight, setpoint);
    int32_t prop_input = weighted_sp - measurement;
    int32_t proportional = fixed_multiply(pid->Kp, prop_input);
    
    // Integral term with clamping
    // integral += error * dt / SCALE (for proper scaling)
    int64_t integral_increment = ((int64_t)error * (int64_t)dt) >> PID_FIXED_POINT_SHIFT;
    pid->integral += integral_increment;
    
    // Clamp integral
    if (pid->integral > pid->integral_max) pid->integral = pid->integral_max;
    if (pid->integral < pid->integral_min) pid->integral = pid->integral_min;
    
    int32_t integral = (int32_t)((pid->integral * pid->Ki) >> PID_FIXED_POINT_SHIFT);
    
    // Derivative term on measurement
    int32_t derivative = 0;
    if (pid->initialized) {
        int32_t d_meas = ((measurement - pid->prev_measurement) << PID_FIXED_POINT_SHIFT) / dt;
        
        // Low-pass filter (simplified)
        static int32_t d_filtered = 0;
        d_filtered = fixed_multiply(d_filtered, pid->derivative_filter) + 
                     fixed_multiply(d_meas, FIXED_ONE - pid->derivative_filter);
        
        derivative = -fixed_multiply(pid->Kd, d_filtered);
    }
    
    // Calculate output
    int32_t output = proportional + integral + derivative;
    
    // Output saturation with back-calculation
    int32_t saturated = fast_clamp_int32(output, pid->output_min, pid->output_max);
    
    // Back-calculate integral if saturated
    if (saturated != output && pid->Ki != 0) {
        int32_t back_calc = saturated - proportional - derivative;
        pid->integral = ((int64_t)back_calc << PID_FIXED_POINT_SHIFT) / pid->Ki;
        
        if (pid->integral > pid->integral_max) pid->integral = pid->integral_max;
        if (pid->integral < pid->integral_min) pid->integral = pid->integral_min;
    }
    
    // Store for next iteration
    pid->prev_error = error;
    pid->prev_measurement = measurement;
    pid->initialized = true;
    
    return saturated;
}

void PID_Fixed_Reset(PID_FixedPoint_t* pid)
{
    if (pid == NULL) return;
    
    pid->integral = 0;
    pid->prev_error = 0;
    pid->prev_measurement = 0;
    pid->initialized = false;
}

void PID_Fixed_SetGains(PID_FixedPoint_t* pid, float Kp, float Ki, float Kd)
{
    if (pid == NULL) return;
    
    pid->Kp = float_to_fixed(Kp);
    pid->Ki = float_to_fixed(Ki);
    pid->Kd = float_to_fixed(Kd);
}

float PID_Fixed_GetOutputFloat(int32_t fixed_output)
{
    return fixed_to_float(fixed_output);
}

#endif // PID_FIXED_POINT_ENABLED
