/**
 * @file config_optimized.h
 * @brief Optimized system configuration for Performance Optimization
 * 
 * Clock Configuration (16MHz HSE):
 *   SYSCLK = 84 MHz
 *   HCLK   = 84 MHz
 *   PCLK1  = 42 MHz (APB1)
 *   PCLK2  = 84 MHz (APB2)
 * 
 * @version 2.3.2-OPT
 * @date 2026-04-16
 */

#ifndef CONFIG_OPTIMIZED_H
#define CONFIG_OPTIMIZED_H

#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// PERFORMANCE OPTIMIZATION SETTINGS
// =============================================================================

// Enable cycle-count instrumentation for profiling
#define PERF_CYCLE_COUNT_ENABLED        1

// Enable DMA double-buffering for ADC
#define ADC_DMA_DOUBLE_BUFFER_ENABLED   1

// Enable fixed-point PID calculations
#define PID_FIXED_POINT_ENABLED         1

// Enable compiler optimizations hints
#define FORCE_INLINE                    __attribute__((always_inline)) inline
#define HOT_FUNC                        __attribute__((hot))
#define FAST_FUNC                       __attribute__((hot, section(".fast")))

// =============================================================================
// CLOCK CONFIGURATION
// =============================================================================

#define HSE_FREQ_HZ                     16000000UL
#define SYSCLK_FREQ_HZ                  84000000UL
#define HCLK_FREQ_HZ                    84000000UL
#define PCLK1_FREQ_HZ                   42000000UL
#define PCLK2_FREQ_HZ                   84000000UL

// =============================================================================
// PWM CONFIGURATION - Optimized
// =============================================================================

#define PWM_FREQUENCY_HZ                20000
#define PWM_DEAD_TIME_NS                400

// Duty cycle limits
#define PWM_HARD_MIN_DUTY               0.02f
#define PWM_HARD_MAX_DUTY               0.98f
#define PWM_SOFT_MIN_DUTY               0.05f
#define PWM_SOFT_MAX_DUTY               0.95f
#define PWM_DUTY_HYSTERESIS             0.005f

// Setpoint ramping
#define PWM_RAMP_ENABLED                1
#define PWM_RAMP_RATE_PER_SEC           0.10f

// PWM Timer: TIM1 on APB2 (84 MHz)
#define PWM_ARR_VALUE(freq)             ((SYSCLK_FREQ_HZ / (freq)) - 1)

// =============================================================================
// ADC CONFIGURATION - Optimized with DMA double-buffering
// =============================================================================

#define ADC_CLOCK_HZ                    (PCLK2_FREQ_HZ / 2)
#define ADC_SAMPLE_RATE_HZ              10000
#define ADC_NUM_CHANNELS                4

// DMA double-buffer configuration
#define ADC_DMA_BUFFER_SAMPLES          32      // Samples per half-buffer
#define ADC_DMA_BUFFER_SIZE             (ADC_DMA_BUFFER_SAMPLES * ADC_NUM_CHANNELS * 2)  // Full buffer

// ADC reference and resolution
#define ADC_VREF_MV                     3300.0f
#define ADC_RESOLUTION                  4096.0f

// Optimized filtering - faster IIR for lower latency
#define ADC_FILTER_IIR_ENABLED          1
#define ADC_FILTER_IIR_ALPHA            0.15f   // Faster response than 0.1

// Moving average for noise rejection
#define ADC_FILTER_MOVING_AVG_ENABLED   1
#define ADC_FILTER_MOVING_AVG_SIZE      4       // Reduced from 8 for lower latency

// Adaptive sampling
#define ADC_ADAPTIVE_SAMPLING_ENABLED   1
#define ADC_TRANSIENT_THRESHOLD         0.05f
#define ADC_FAST_SAMPLE_RATE_HZ         20000
#define ADC_FAST_SAMPLE_DURATION_MS     100

// Optimized sampling times
#define ADC_SAMPLETIME_FAST             3       // 3 cycles = 71ns @ 42MHz
#define ADC_SAMPLETIME_MEDIUM           15      // 15 cycles = 357ns
#define ADC_SAMPLETIME_SLOW             28      // 28 cycles = 667ns

// Total conversion time with 4 channels:
// (3+12) + (3+12) + (15+12) + (28+12) = 15+15+27+40 = 97 cycles @ 42MHz = 2.3µs
// DMA overhead: ~0.5µs per transfer
// Total sample time: ~4.7µs per set of 4 channels

// =============================================================================
// UART CONFIGURATION
// =============================================================================

#define UART_BAUDRATE                   115200
#define UART_TX_BUFFER_SIZE             256
#define UART_RX_BUFFER_SIZE             256

// =============================================================================
// SAFETY LIMITS
// =============================================================================

#define VOLTAGE_MIN_V                   5.0f
#define VOLTAGE_MAX_V                   30.0f
#define VOLTAGE_WARNING_LOW_V           6.0f
#define VOLTAGE_WARNING_HIGH_V          28.0f

#define CURRENT_MAX_A                   10.0f
#define CURRENT_WARNING_A               8.0f
#define CURRENT_SENSE_OHMS              0.01f

#define TEMP_WARNING_C                  75.0f
#define TEMP_CRITICAL_C                 85.0f
#define TEMP_SHUTDOWN_C                 95.0f
#define TEMP_HYSTERESIS_C               2.0f

// =============================================================================
// CONTROL PARAMETERS - Optimized
// =============================================================================

#define TARGET_EFFICIENCY               0.95f
#define EFFICIENCY_MIN_ACCEPTABLE       0.85f

// PID gains
#define DUTY_KP                         0.05f
#define DUTY_KI                         0.01f
#define DUTY_KD                         0.001f

// PID setpoint weighting
#define PID_SETPOINT_WEIGHT             0.7f
#define PID_DERIVATIVE_FILTER           0.1f

// Feedforward
#define FEEDFORWARD_ENABLED             1
#define FEEDFORWARD_GAIN                0.5f

// =============================================================================
// FIXED-POINT PID CONFIGURATION
// =============================================================================

#if PID_FIXED_POINT_ENABLED
// Fixed-point Q16.16 format (32-bit total, 16-bit fractional)
#define PID_FIXED_POINT_SHIFT           16
#define PID_FIXED_POINT_SCALE           (1 << PID_FIXED_POINT_SHIFT)
#define PID_FLOAT_TO_FIXED(f)           ((int32_t)((f) * PID_FIXED_POINT_SCALE))
#define PID_FIXED_TO_FLOAT(i)           ((float)(i) / PID_FIXED_POINT_SCALE)
#define PID_FIXED_MUL(a, b)             (((int64_t)(a) * (b)) >> PID_FIXED_POINT_SHIFT)

// Maximum fixed-point values
#define PID_FIXED_MAX                   INT32_MAX
#define PID_FIXED_MIN                   INT32_MIN
#endif

// =============================================================================
// TASK TIMING REQUIREMENTS (for jitter analysis)
// =============================================================================

#define TASK_CYCLE_TIME_MEASURE_US      1000    // 1ms = 1kHz
#define TASK_CYCLE_TIME_CONTROL_US      10000   // 10ms = 100Hz
#define TASK_CYCLE_TIME_SAFETY_US       10000   // 10ms = 100Hz

// Maximum allowed jitter (acceptance criteria)
#define MAX_JITTER_US                   10      // 10µs max jitter
#define MAX_ADC_TO_PWM_LATENCY_US       50      // 50µs max ADC-to-PWM latency

// =============================================================================
// PERFORMANCE PROFILING
// =============================================================================

#if PERF_CYCLE_COUNT_ENABLED
#include "performance_profiler.h"
#endif

#endif // CONFIG_OPTIMIZED_H
