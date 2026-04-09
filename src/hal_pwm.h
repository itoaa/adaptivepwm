/**
 * @file hal_pwm.h
 * @brief Hardware Abstraction Layer for PWM generation
 * 
 * Handles TIM1 configuration for complementary PWM outputs
 * with dead-time insertion for H-bridge/buck-boost control.
 * 
 * Features:
 * - Duty cycle hysteresis to prevent flutter
 * - Emergency stop with hardware break input
 * - Hardware and software duty limits
 * 
 * @version 2.2.0
 * @date 2026-04-09
 */

#ifndef HAL_PWM_H
#define HAL_PWM_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "config.h"

// PWM GPIO Configuration
#define PWM_TIMER           TIM1
#define PWM_GPIO_PORT       GPIOA
#define PWM_GPIO_PIN_CH1    GPIO_PIN_8   // PA8 - TIM1_CH1
#define PWM_GPIO_PIN_CH2    GPIO_PIN_9   // PA9 - TIM1_CH2 (complementary)
#define PWM_GPIO_AF         GPIO_AF1_TIM1

/**
 * @brief PWM handle structure with hysteresis support
 */
typedef struct {
    TIM_HandleTypeDef htim;
    uint32_t frequency;
    uint16_t period;
    volatile uint16_t current_duty;   ///< Current duty cycle in timer ticks
    float last_duty_set;                ///< Last duty cycle set (0.0-1.0), for hysteresis
    float hysteresis_threshold;         ///< Minimum change before update (prevents flutter)
    volatile bool is_running;
} Adaptive_PWM_t;

// NOTE: Using Adaptive_ prefix to avoid conflicts with STM32 HAL functions

/**
 * @brief Initialize PWM hardware
 * @param pwm Pointer to PWM handle structure
 * @return true on success, false on failure
 */
bool Adaptive_PWM_Init(Adaptive_PWM_t* pwm);

/**
 * @brief Start PWM output
 * @param pwm Pointer to initialized PWM handle
 * @return true on success, false on failure
 */
bool Adaptive_PWM_Start(Adaptive_PWM_t* pwm);

/**
 * @brief Stop PWM output
 * @param pwm Pointer to running PWM handle
 * @return true on success, false on failure
 */
bool Adaptive_PWM_Stop(Adaptive_PWM_t* pwm);

/**
 * @brief Set duty cycle with hysteresis
 * 
 * Only updates if change is greater than hysteresis_threshold.
 * This prevents duty cycle flutter at boundary values.
 * 
 * @param pwm Pointer to running PWM handle
 * @param duty Duty cycle (0.0 - 1.0)
 * @return true on success, false on failure
 */
bool Adaptive_PWM_SetDuty(Adaptive_PWM_t* pwm, float duty);

/**
 * @brief Get current duty cycle
 * @param pwm Pointer to PWM handle
 * @return Current duty cycle (0.0 - 1.0)
 */
float Adaptive_PWM_GetDuty(const Adaptive_PWM_t* pwm);

/**
 * @brief Emergency stop via hardware break input
 * 
 * Immediately stops PWM and forces outputs to inactive state.
 * Requires reset to recover.
 * 
 * @param pwm Pointer to PWM handle
 */
void Adaptive_PWM_EmergencyStop(Adaptive_PWM_t* pwm);

/**
 * @brief Get PWM frequency
 * @param pwm Pointer to PWM handle
 * @return Frequency in Hz
 */
uint32_t Adaptive_PWM_GetFrequency(const Adaptive_PWM_t* pwm);

/**
 * @brief Check if PWM is running
 * @param pwm Pointer to PWM handle
 * @return true if running, false otherwise
 */
bool Adaptive_PWM_IsRunning(const Adaptive_PWM_t* pwm);

#endif // HAL_PWM_H
