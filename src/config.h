/**
 * @file config.h
 * @brief Central system configuration - Optimized Clock System
 *
 * Feature toggles (security, experimental estimators) live in:
 *   config/features.h  — defaults favor bring-up over hardening
 *
 * Clock Configuration (16MHz HSE):
 *   HSE = 16 MHz (external crystal)
 *   PLL: M=16, N=336, P=4, Q=7
 *   SYSCLK = 84 MHz
 *   HCLK   = 84 MHz
 *   PCLK1  = 42 MHz (APB1 - ADC, UART, TIM2-5)
 *   PCLK2  = 84 MHz (APB2 - TIM1, ADC)
 *
 * @version 2.3.2
 * @date 2026-08-08
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/* Master feature switches (CLI auth, HMAC, ripple L/C, etc.) */
#include "features.h"

// =============================================================================
// VERSION
// =============================================================================
#define ADAPTIVEPWM_VERSION_MAJOR   2
#define ADAPTIVEPWM_VERSION_MINOR   3
#define ADAPTIVEPWM_VERSION_PATCH   2
#define ADAPTIVEPWM_VERSION_STRING  "2.3.2"

// =============================================================================
// CLOCK CONFIGURATION
// =============================================================================

// External crystal frequency
#define HSE_FREQ_HZ                 16000000UL  // 16 MHz external crystal

// System clock (after PLL)
#define SYSCLK_FREQ_HZ              84000000UL  // 84 MHz

// Bus clocks
#define HCLK_FREQ_HZ                84000000UL  // AHB = 84 MHz
#define PCLK1_FREQ_HZ               42000000UL  // APB1 = 42 MHz (max)
#define PCLK2_FREQ_HZ               84000000UL  // APB2 = 84 MHz

// PLL Configuration
// SYSCLK = HSE / PLLM * PLLN / PLLP
// 84 MHz = 16 MHz / 16 * 336 / 4
#define PLLM_VALUE                  16
#define PLLN_VALUE                  336
#define PLLP_VALUE                  4
#define PLLQ_VALUE                  7   // USB = 48 MHz

// =============================================================================
// RNG CONFIGURATION (SEC-033 - Hardware RNG)
// RNG_ENABLED comes from config/features.h (0 on STM32F401)
// =============================================================================

// RNG clock: AHB2 bus clock (HCLK)
#define RNG_CLOCK_ENABLE()          __HAL_RCC_RNG_CLK_ENABLE()
#define RNG_CLOCK_DISABLE()         __HAL_RCC_RNG_CLK_DISABLE()

#ifndef RNG_ERROR_RECOVERY
    #define RNG_ERROR_RECOVERY      1
#endif

/* Software fallback required on F401 (no HW RNG). Only used if auth/crypto runs. */
#ifndef RNG_FALLBACK_SOFTWARE
    #define RNG_FALLBACK_SOFTWARE   1
#endif

#ifndef RNG_TIMEOUT_MS
    #define RNG_TIMEOUT_MS          100
#endif

#ifndef RNG_MAX_ATTEMPTS
    #define RNG_MAX_ATTEMPTS        3
#endif

#ifndef SETUP_TIMEOUT_MS
    #define SETUP_TIMEOUT_MS        30000  // 30 seconds
#endif

// =============================================================================
// PWM CONFIGURATION
// =============================================================================

#define PWM_FREQUENCY_HZ            20000       // 20 kHz switching
#define PWM_DEAD_TIME_NS            400         // 400 ns dead-time

// Duty cycle limits
#define PWM_HARD_MIN_DUTY           0.02f       // Absolute minimum (safety)
#define PWM_HARD_MAX_DUTY           0.98f       // Absolute maximum (safety)
#define PWM_SOFT_MIN_DUTY           0.05f       // Normal minimum
#define PWM_SOFT_MAX_DUTY           0.95f       // Normal maximum

// Duty cycle hysteresis to prevent flutter
#define PWM_DUTY_HYSTERESIS         0.005f      // 0.5% hysteresis

// Setpoint ramping for smooth transitions
#define PWM_RAMP_ENABLED            1
#define PWM_RAMP_RATE_PER_SEC       0.10f       // 10% per second max change

// PWM Timer: TIM1 on APB2 (84 MHz)
// ARR = 84MHz / 20kHz - 1 = 4199
// Resolution: 12-bit equivalent (4200 steps)
#define PWM_ARR_VALUE(freq)       ((SYSCLK_FREQ_HZ / (freq)) - 1)

// =============================================================================
// ADC CONFIGURATION
// =============================================================================

// ADC Clock: PCLK2 / 2 = 42 MHz (maximum allowed)
#define ADC_CLOCK_HZ                (PCLK2_FREQ_HZ / 2)

// Sampling rate
#define ADC_SAMPLE_RATE_HZ          10000       // Total sampling rate
#define ADC_NUM_CHANNELS            4
#define ADC_DMA_BUFFER_SIZE         64

// ADC reference and resolution
#define ADC_VREF_MV                 3300.0f     // 3.3V reference
#define ADC_RESOLUTION              4096.0f     // 12-bit (0-4095)

/*
 * Front-end scaling (adjust for your power board via build flags or edit).
 * V_actual = V_adc * DIVIDER_RATIO  (e.g. 100k/10k → 11.0)
 * I_actual = V_adc / CURRENT_V_PER_AMP  (sense amp: volts per amp at ADC pin)
 *
 * Defaults = 1.0 / gentle current scale for Nucleo bring-up without dividers.
 * Power stage example: -DADC_VIN_DIVIDER_RATIO=11 -DADC_VOUT_DIVIDER_RATIO=11
 *                      -DADC_CURRENT_V_PER_AMP=0.1f
 */
#ifndef ADC_VIN_DIVIDER_RATIO
#define ADC_VIN_DIVIDER_RATIO       1.0f
#endif
#ifndef ADC_VOUT_DIVIDER_RATIO
#define ADC_VOUT_DIVIDER_RATIO      1.0f
#endif
#ifndef ADC_CURRENT_V_PER_AMP
#define ADC_CURRENT_V_PER_AMP       1.0f   /* 1 V at pin → 1 A (lab default) */
#endif

// ADC filtering options
#define ADC_FILTER_IIR_ENABLED      1
#define ADC_FILTER_IIR_ALPHA        0.1f        // IIR filter coefficient
#define ADC_FILTER_MOVING_AVG_ENABLED 1
#define ADC_FILTER_MOVING_AVG_SIZE  8           // 8-sample moving average

// Adaptive sampling - increase rate during transients
#define ADC_ADAPTIVE_SAMPLING_ENABLED 1
#define ADC_TRANSIENT_THRESHOLD     0.05f       // 5% change triggers fast sampling
#define ADC_FAST_SAMPLE_RATE_HZ     20000       // Double rate during transients
#define ADC_FAST_SAMPLE_DURATION_MS 100         // Duration of fast sampling

// Sampling times (in cycles) - optimized for 42 MHz ADC clock
#define ADC_SAMPLETIME_FAST         3
#define ADC_SAMPLETIME_MEDIUM       15
#define ADC_SAMPLETIME_SLOW         28

// Total conversion time = sampling + 12 cycles (resolution)
// Fast:   15 cycles @ 42 MHz = 357 ns
// Medium: 27 cycles @ 42 MHz = 643 ns
// Slow:   40 cycles @ 42 MHz = 952 ns

// =============================================================================
// UART CONFIGURATION
// =============================================================================

#define UART_BAUDRATE               115200
#define UART_TX_BUFFER_SIZE         256
#define UART_RX_BUFFER_SIZE         256

// UART Clock: PCLK1 = 42 MHz
// Baud rate error at 115200: ~0.16% (acceptable)

// =============================================================================
// CLI AUTHENTICATION CONFIGURATION (SEC-019)
// CLI_AUTH_ENABLED comes from config/features.h (default OFF for bring-up)
// =============================================================================

#ifndef CLI_AUTH_MAX_ATTEMPTS
    #define CLI_AUTH_MAX_ATTEMPTS     3
#endif

#ifndef CLI_AUTH_LOCKOUT_DURATION_S
    #define CLI_AUTH_LOCKOUT_DURATION_S  300
#endif

#ifndef CLI_AUTH_PASSWORD_MIN_LEN
    #define CLI_AUTH_PASSWORD_MIN_LEN    4
#endif

#ifndef CLI_AUTH_PASSWORD_MAX_LEN
    #define CLI_AUTH_PASSWORD_MAX_LEN    32
#endif

/* High iteration count only matters when FEATURE_CLI_AUTH=1 */
#ifndef CLI_AUTH_HASH_ITERATIONS
    #define CLI_AUTH_HASH_ITERATIONS     100000
#endif

#if FEATURE_SECURITY_STRICT && (CLI_AUTH_HASH_ITERATIONS < 100000)
    #warning "CLI_AUTH_HASH_ITERATIONS below NIST SP 800-132 recommended minimum of 100,000"
#endif

#ifndef CLI_AUTH_SESSION_TIMEOUT_S
    #define CLI_AUTH_SESSION_TIMEOUT_S   300  // 5 minutes
#endif

// =============================================================================
// SAFETY LIMITS
// =============================================================================

/*
 * Voltage limits in engineering units after divider scale.
 * With lab defaults (divider=1), limits apply to 0–3.3 V ADC pin domain —
 * VIN UV is disabled below when FEATURE bring-up: use VIN_UV_ENABLE.
 */
#define VOLTAGE_MIN_V               0.5f        /* lab-friendly; raise for real bus */
#define VOLTAGE_MAX_V               30.0f
#define VOLTAGE_WARNING_LOW_V       0.6f
#define VOLTAGE_WARNING_HIGH_V      28.0f
#define VOLTAGE_UV_HYSTERESIS_V     0.2f
#define SAFETY_VOUT_UV_ENABLE_MS    500U
#define SAFETY_FAULT_COOLDOWN_MS    2000U
/* 0 = do not trip on VIN UV (useful when Vin not wired yet); 1 = enforce */
#ifndef SAFETY_VIN_UV_ENABLE
#define SAFETY_VIN_UV_ENABLE        0
#endif

// Current limits
#define CURRENT_MAX_A               10.0f
#define CURRENT_WARNING_A           8.0f
#define CURRENT_SENSE_OHMS          0.01f       // 10 mOhm shunt (info; scale via ADC_CURRENT_V_PER_AMP)

// Temperature limits
#define TEMP_WARNING_C              75.0f
#define TEMP_CRITICAL_C             85.0f
#define TEMP_SHUTDOWN_C             95.0f
#define TEMP_HYSTERESIS_C           2.0f        // Prevent oscillation

// =============================================================================
// CONTROL PARAMETERS
// =============================================================================

// Primary: regulate output voltage (FEATURE_VOUT_CONTROL)
// Lab default 1.5 V fits 0–3.3 V pin domain; set 12 V on real converters.
#ifndef VOUT_SETPOINT_DEFAULT_V
#define VOUT_SETPOINT_DEFAULT_V     1.5f
#endif

// Efficiency target (only if FEATURE_EFFICIENCY_CONTROL)
#define TARGET_EFFICIENCY           0.95f
#define EFFICIENCY_MIN_ACCEPTABLE   0.85f

// Control loop gains (Vout PID / duty)
#define DUTY_KP                     0.05f
#define DUTY_KI                     0.01f
#define DUTY_KD                     0.001f

// Setpoint weighting - reduces overshoot while maintaining speed
#define PID_SETPOINT_WEIGHT         0.7f        // 0-1, lower = less overshoot

// Derivative filter - smooths derivative term
#define PID_DERIVATIVE_FILTER       0.1f        // 0-1, higher = more filtering

// Feedforward for buck/boost converters
// D_estimated = Vout / Vin for buck, 1 - Vout/Vin for boost
#define FEEDFORWARD_ENABLED         1
#define FEEDFORWARD_GAIN            0.5f        // Blend factor (0-1)

// PID Controller Structure with Anti-Windup
typedef struct {
    float Kp, Ki, Kd;            // Gains
    float setpoint_weight;       // Setpoint weighting (0-1)
    float derivative_filter;     // Low-pass filter for derivative (0-1)
    float integral;              // Integral accumulator
    float integral_min, integral_max; // Anti-windup limits
    float prev_error;            // Previous error
    float prev_measurement;      // Previous measurement (derivative on measurement)
    float d_filtered;            // Filtered d/dt(measurement) state
    float output_min, output_max; // Output limits
    bool initialized;            // First run flag
} PID_Controller_t;

// PID Functions
void PID_Init(PID_Controller_t* pid, float Kp, float Ki, float Kd, float output_min, float output_max);
float PID_Compute(PID_Controller_t* pid, float setpoint, float measurement, float dt);
void PID_Reset(PID_Controller_t* pid);
void PID_SetGains(PID_Controller_t* pid, float Kp, float Ki, float Kd);
void PID_SetSetpointWeight(PID_Controller_t* pid, float weight);
void PID_SetDerivativeFilter(PID_Controller_t* pid, float alpha);
float PID_GetIntegral(const PID_Controller_t* pid);
void PID_SetIntegral(PID_Controller_t* pid, float integral);

// =============================================================================
// FLASH LOGGER CONFIGURATION
// =============================================================================

#define FLASH_LOG_SECTOR            FLASH_SECTOR_7
#define FLASH_LOG_START_ADDR        0x080E0000
#define FLASH_LOG_SIZE              8192
#define FLASH_LOG_ENTRY_SIZE        32
#define FLASH_MAGIC                 0xAD4DA1FEUL

// Wear leveling: spread writes across sector
#define FLASH_WEAR_LEVELING_ENABLED 1
#define FLASH_LOG_ENTRIES_PER_SECTOR (FLASH_LOG_SIZE / FLASH_LOG_ENTRY_SIZE)

// =============================================================================
// FLASH LOGGER HMAC-SHA256 (SEC-023) — optional, see features.h
// FLASH_LOGGER_HMAC_ENABLED comes from config/features.h (default OFF)
// =============================================================================

#define HMAC_SHA256_KEY_SIZE        32
#define HMAC_SHA256_SIGNATURE_SIZE  32
#define HMAC_SALT_SIZE              16

#define HMAC_KEY_FLASH_ADDR         0x080D0000
#define HMAC_KEY_SECTOR             FLASH_SECTOR_6

#define FLASH_LOG_HMAC_MAGIC        0x484D4143  // "HMAC"
#define FLASH_LOG_CHAIN_MAGIC       0x43484131  // "CHA1"

#define FLASH_HMAC_ENTRY_SIZE       80
#define FLASH_HMAC_HEADER_SIZE      64
#define FLASH_HMAC_MAX_ENTRIES      ((FLASH_LOG_SIZE - FLASH_HMAC_HEADER_SIZE) / FLASH_HMAC_ENTRY_SIZE)

// =============================================================================
// WATCHDOG CONFIGURATION
// =============================================================================

#define WDG_TIMEOUT_MS              500
#define WDG_REFRESH_INTERVAL_MS     100

// =============================================================================
// DEBUG / ASSERT
// =============================================================================

#ifdef DEBUG
    #define ASSERT_ENABLED          1
    #define DEBUG_PRINT_ENABLED     1
#else
    #define ASSERT_ENABLED          0
    #define DEBUG_PRINT_ENABLED     0
#endif

// =============================================================================
// SECURITY FEATURE FLAGS (aliases — source of truth is config/features.h)
// =============================================================================

#define FEATURE_FLASH_HMAC          FEATURE_FLASH_LOGGER_HMAC
#define FEATURE_UART_AUTH           FEATURE_CLI_AUTH
#define FEATURE_WATCHDOG_TASK       0

#define SECURITY_TAMPER_RESPONSE    0   // 0=Log only, 1=Halt on tamper
#define SECURITY_KEY_ROTATION_DAYS  365

// =============================================================================
// FLASH MEMORY MAP
// =============================================================================

/*
 * STM32F401RE Flash Layout (512 KB):
 * 
 * Sector 0: 0x08000000 - 0x08003FFF (16 KB)  - Bootloader/Application
 * Sector 1: 0x08004000 - 0x08007FFF (16 KB)  - Application
 * Sector 2: 0x08008000 - 0x0800BFFF (16 KB)  - Application
 * Sector 3: 0x0800C000 - 0x0800FFFF (16 KB)  - Application
 * Sector 4: 0x08010000 - 0x0801FFFF (64 KB)  - Application
 * Sector 5: 0x08020000 - 0x0803FFFF (128 KB) - Application / Auth Credentials (0x080C0000)
 * Sector 6: 0x08040000 - 0x0805FFFF (128 KB)  - HMAC Key Storage (0x080D0000)
 * Sector 7: 0x08060000 - 0x0807FFFF (128 KB) - Flash Logger (0x080E0000)
 * 
 * Note: Actual addresses adjusted to use upper sectors for data
 * Sector 5: Auth Credentials @ 0x080C0000 (alias)
 * Sector 6: HMAC Key @ 0x080D0000 (alias)
 * Sector 7: Flash Log @ 0x080E0000 (alias)
 */


// =============================================================================
// SETUP GPIO CONFIGURATION (SEC-031)
// SETUP_CONFIRM_ENABLED comes from config/features.h (default OFF)
// =============================================================================

#define SETUP_CONFIRM_GPIO_PORT     GPIOC
#define SETUP_CONFIRM_GPIO_PIN      GPIO_PIN_13

#define SETUP_CONFIRM_ALT_GPIO_PORT GPIOA
#define SETUP_CONFIRM_ALT_GPIO_PIN  GPIO_PIN_0

#define SETUP_BUTTON_DEBOUNCE_MS    50
#define SETUP_BUTTON_PRESS_MIN_MS   2000
#define SETUP_CONFIRM_MODE          0   // 0=Button, 1=Jumper, 2=Auto

#endif // CONFIG_H
