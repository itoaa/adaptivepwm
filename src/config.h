/**
 * @file config.h
 * @brief Central system configuration - Optimized Clock System
 * 
 * Clock Configuration (16MHz HSE):
 *   HSE = 16 MHz (external crystal)
 *   PLL: M=16, N=336, P=4, Q=7
 *   SYSCLK = 84 MHz
 *   HCLK   = 84 MHz
 *   PCLK1  = 42 MHz (APB1 - ADC, UART, TIM2-5)
 *   PCLK2  = 84 MHz (APB2 - TIM1, ADC)
 * 
 * @version 2.1.0
 * @date 2026-03-21
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// VERSION
// =============================================================================
#define ADAPTIVEPWM_VERSION_MAJOR   2
#define ADAPTIVEPWM_VERSION_MINOR   1
#define ADAPTIVEPWM_VERSION_PATCH   0
#define ADAPTIVEPWM_VERSION_STRING  "2.1.0"

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
// PWM CONFIGURATION
// =============================================================================

#define PWM_FREQUENCY_HZ            20000       // 20 kHz switching
#define PWM_DEAD_TIME_NS            400         // 400 ns dead-time

// Duty cycle limits
#define PWM_HARD_MIN_DUTY           0.02f       // Absolute minimum (safety)
#define PWM_HARD_MAX_DUTY           0.98f       // Absolute maximum (safety)
#define PWM_SOFT_MIN_DUTY           0.05f       // Normal minimum
#define PWM_SOFT_MAX_DUTY           0.95f       // Normal maximum

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

// Sampling times (in cycles) - optimized for 42 MHz ADC clock
// 3 cycles  = 71 ns   (fastest, for low impedance)
// 15 cycles = 357 ns  (medium, for current sense)
// 28 cycles = 667 ns  (slower, for temperature)
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
// SAFETY LIMITS
// =============================================================================

// Voltage limits
#define VOLTAGE_MIN_V               5.0f
#define VOLTAGE_MAX_V               30.0f
#define VOLTAGE_WARNING_LOW_V       6.0f
#define VOLTAGE_WARNING_HIGH_V      28.0f

// Current limits
#define CURRENT_MAX_A               10.0f
#define CURRENT_WARNING_A           8.0f
#define CURRENT_SENSE_OHMS          0.01f       // 10 mOhm shunt

// Temperature limits
#define TEMP_WARNING_C              75.0f
#define TEMP_CRITICAL_C             85.0f
#define TEMP_SHUTDOWN_C               95.0f
#define TEMP_HYSTERESIS_C           2.0f        // Prevent oscillation

// =============================================================================
// CONTROL PARAMETERS
// =============================================================================

// Efficiency target
#define TARGET_EFFICIENCY           0.95f
#define EFFICIENCY_MIN_ACCEPTABLE   0.85f

// Control loop gains
#define DUTY_KP                     0.05f       // Proportional gain
#define DUTY_KI                     0.0f        // Integral gain (unused)
#define DUTY_KD                     0.0f        // Derivative gain (unused)

// Measurement smoothing
#define MEASUREMENT_ALPHA           0.1f        // IIR filter coefficient

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

#endif // CONFIG_H
