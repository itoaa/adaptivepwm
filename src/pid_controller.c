/**
 * @file pid_controller.c
 * @brief PID Controller Implementation with Anti-Windup and Derivative on Measurement
 * 
 * Full PID controller with integral windup prevention and derivative on measurement
 * to prevent derivative kick on setpoint changes. Used for efficiency-based duty cycle control.
 * 
 * Features:
 * - Proportional, Integral, Derivative terms
 * - Anti-windup via integral clamping and back-calculation
 * - Derivative on measurement (prevents derivative kick)
 * - Setpoint weighting for proportional term
 * - Output saturation limits
 * - Smooth derivative with filtering
 * 
 * @version 2.2.1
 * @date 2026-04-10
 */

#include "config.h"
#include "adaptive_assert.h"
#include <string.h>
#include <math.h>

void PID_Init(PID_Controller_t* pid, float Kp, float Ki, float Kd, float output_min, float output_max)
{
    ADAPTIVE_ASSERT(pid != NULL);
    ADAPTIVE_ASSERT(output_min < output_max);
    
    if (pid == NULL) return;
    
    pid->Kp = Kp;
    pid->Ki = Ki;
    pid->Kd = Kd;
    pid->output_min = output_min;
    pid->output_max = output_max;
    pid->setpoint_weight = 1.0f;  // Default: full setpoint weighting
    pid->derivative_filter = 0.1f;  // Low-pass filter for derivative
    
    // Calculate integral limits (conservative anti-windup)
    // Max integral contribution should be about half of output range
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
    pid->d_filtered = 0.0f;
    pid->initialized = false;
    
    DEBUG_PRINT("PID: Initialized Kp=%.4f Ki=%.4f Kd=%.4f limits=[%.3f, %.3f]",
                Kp, Ki, Kd, output_min, output_max);
}

float PID_Compute(PID_Controller_t* pid, float setpoint, float measurement, float dt)
{
    ADAPTIVE_ASSERT(pid != NULL);
    ADAPTIVE_ASSERT(dt > 0.0f);
    
    if (pid == NULL || dt <= 0.0f) return 0.0f;
    
    // Calculate error
    float error = setpoint - measurement;
    
    // Proportional term with setpoint weighting
    // P = Kp * (b*setpoint - measurement) where b is setpoint weight (0-1)
    // This reduces overshoot while maintaining fast response
    float proportional = pid->Kp * (pid->setpoint_weight * setpoint - measurement);
    
    // Integral term with anti-windup
    pid->integral += error * dt;
    
    // Clamp integral to prevent windup
    if (pid->integral > pid->integral_max) {
        pid->integral = pid->integral_max;
    } else if (pid->integral < pid->integral_min) {
        pid->integral = pid->integral_min;
    }
    
    float integral = pid->Ki * pid->integral;
    
    // Derivative term (derivative on measurement, not error)
    // Filter state is per-instance (pid->d_filtered), not file-static.
    float derivative = 0.0f;
    if (pid->initialized) {
        float d_measurement = (measurement - pid->prev_measurement) / dt;
        pid->d_filtered = pid->d_filtered * pid->derivative_filter +
                          d_measurement * (1.0f - pid->derivative_filter);
        derivative = -pid->Kd * pid->d_filtered;
    }
    
    // Calculate output
    float output = proportional + integral + derivative;
    
    // Output saturation with back-calculation anti-windup
    float saturated_output = output;
    bool saturated = false;
    if (output > pid->output_max) {
        saturated_output = pid->output_max;
        saturated = true;
    } else if (output < pid->output_min) {
        saturated_output = pid->output_min;
        saturated = true;
    }
    
    // Back-calculate integral to prevent windup only when saturated
    if (saturated && pid->Ki > 1e-6f) {
        pid->integral = (saturated_output - proportional - derivative) / pid->Ki;
        // Re-clamp after back-calculation
        if (pid->integral > pid->integral_max) {
            pid->integral = pid->integral_max;
        } else if (pid->integral < pid->integral_min) {
            pid->integral = pid->integral_min;
        }
    }
    
    // Store for next iteration
    pid->prev_error = error;
    pid->prev_measurement = measurement;
    pid->initialized = true;
    
    DEBUG_PRINT_EVERY_N(100, "PID: SP=%.3f MV=%.3f Err=%.3f P=%.3f I=%.3f D=%.3f Out=%.3f",
                        setpoint, measurement, error, proportional, integral, derivative, saturated_output);
    
    return saturated_output;
}

void PID_Reset(PID_Controller_t* pid)
{
    ADAPTIVE_ASSERT(pid != NULL);
    
    if (pid == NULL) return;
    
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->prev_measurement = 0.0f;
    pid->d_filtered = 0.0f;
    pid->initialized = false;
    
    DEBUG_PRINT("PID: Reset");
}

/**
 * @brief Set new PID gains at runtime
 */
void PID_SetGains(PID_Controller_t* pid, float Kp, float Ki, float Kd)
{
    ADAPTIVE_ASSERT(pid != NULL);
    
    if (pid == NULL) return;
    
    pid->Kp = Kp;
    pid->Ki = Ki;
    pid->Kd = Kd;
    
    // Recalculate integral limits
    float range = pid->output_max - pid->output_min;
    if (Ki > 1e-6f) {
        pid->integral_max = range * 0.5f / Ki;
        pid->integral_min = -pid->integral_max;
    } else {
        pid->integral_max = 1e6f;
        pid->integral_min = -1e6f;
    }
}

/**
 * @brief Set setpoint weighting (0.0 to 1.0)
 * Lower values reduce overshoot but may slow response
 */
void PID_SetSetpointWeight(PID_Controller_t* pid, float weight)
{
    ADAPTIVE_ASSERT(pid != NULL);
    ADAPTIVE_ASSERT(weight >= 0.0f && weight <= 1.0f);
    
    if (pid == NULL) return;
    if (weight < 0.0f) weight = 0.0f;
    if (weight > 1.0f) weight = 1.0f;
    
    pid->setpoint_weight = weight;
}

/**
 * @brief Set derivative filter coefficient (0.0 to 1.0)
 * Higher values = more filtering (smoother but slower)
 */
void PID_SetDerivativeFilter(PID_Controller_t* pid, float alpha)
{
    ADAPTIVE_ASSERT(pid != NULL);
    ADAPTIVE_ASSERT(alpha >= 0.0f && alpha <= 1.0f);
    
    if (pid == NULL) return;
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    
    pid->derivative_filter = alpha;
}

/**
 * @brief Get current integral term
 */
float PID_GetIntegral(const PID_Controller_t* pid)
{
    ADAPTIVE_ASSERT(pid != NULL);
    
    if (pid == NULL) return 0.0f;
    return pid->integral;
}

/**
 * @brief Set integral term (useful for manual/auto bumpless transfer)
 */
void PID_SetIntegral(PID_Controller_t* pid, float integral)
{
    ADAPTIVE_ASSERT(pid != NULL);
    
    if (pid == NULL) return;
    
    pid->integral = integral;
    
    // Clamp to limits
    if (pid->integral > pid->integral_max) {
        pid->integral = pid->integral_max;
    } else if (pid->integral < pid->integral_min) {
        pid->integral = pid->integral_min;
    }
}
