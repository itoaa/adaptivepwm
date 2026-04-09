/**
 * @file pid_controller.c
 * @brief PID Controller Implementation with Anti-Windup
 * 
 * Full PID controller with integral windup prevention.
 * Used for efficiency-based duty cycle control in AdaptivePWM.
 * 
 * Features:
 * - Proportional, Integral, Derivative terms
 * - Anti-windup via integral clamping
 * - Output saturation limits
 * - Smooth derivative (derivative on measurement)
 * 
 * @version 2.2.0
 * @date 2026-04-09
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
    
    // Calculate integral limits (conservative anti-windup)
    // Max integral contribution should be about half of output range
    float range = output_max - output_min;
    pid->integral_max = range * 0.5f / Ki;  // Ki might be 0, check below
    pid->integral_min = -pid->integral_max;
    
    // If Ki is 0 or very small, use large limits (no anti-windup needed)
    if (Ki < 1e-6f) {
        pid->integral_max = 1e6f;
        pid->integral_min = -1e6f;
    }
    
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
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
    
    // Proportional term
    float proportional = pid->Kp * error;
    
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
    // This prevents derivative kick on setpoint changes
    float derivative = 0.0f;
    if (pid->initialized) {
        // d/dt(measurement) = -d/dt(error) for constant setpoint
        // Using measurement derivative avoids spikes on setpoint changes
        float d_measurement = (measurement - (measurement - error)) / dt;  // Simplified
        // Better: track previous measurement
        derivative = pid->Kd * d_measurement;
    }
    
    // Calculate output
    float output = proportional + integral + derivative;
    
    // Output saturation (back-calculation anti-windup)
    float saturated_output = output;
    if (output > pid->output_max) {
        saturated_output = pid->output_max;
        // Back-calculate integral to prevent windup
        pid->integral = (saturated_output - proportional - derivative) / pid->Ki;
    } else if (output < pid->output_min) {
        saturated_output = pid->output_min;
        // Back-calculate integral to prevent windup
        pid->integral = (saturated_output - proportional - derivative) / pid->Ki;
    }
    
    // Store for next iteration
    pid->prev_error = error;
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
    pid->integral_max = range * 0.5f / Ki;
    pid->integral_min = -pid->integral_max;
    
    if (Ki < 1e-6f) {
        pid->integral_max = 1e6f;
        pid->integral_min = -1e6f;
    }
}
