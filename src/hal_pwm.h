/**
 * @file hal_pwm.h
 * @brief Hardware Abstraction Layer for PWM generation
 * 
 * Handles TIM1 configuration for complementary PWM outputs
 * with dead-time insertion for H-bridge/buck-boost control.
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

typedef struct {
    TIM_HandleTypeDef htim;
    uint32_t frequency;
    uint16_t period;
    volatile uint16_t current_duty;
    volatile bool is_running;
} Adaptive_PWM_t;

// NOTE: Using Adaptive_ prefix to avoid conflicts with STM32 HAL functions

bool Adaptive_PWM_Init(Adaptive_PWM_t* pwm);
bool Adaptive_PWM_Start(Adaptive_PWM_t* pwm);
bool Adaptive_PWM_Stop(Adaptive_PWM_t* pwm);
bool Adaptive_PWM_SetDuty(Adaptive_PWM_t* pwm, float duty);
float Adaptive_PWM_GetDuty(const Adaptive_PWM_t* pwm);
void Adaptive_PWM_EmergencyStop(Adaptive_PWM_t* pwm);
uint32_t Adaptive_PWM_GetFrequency(const Adaptive_PWM_t* pwm);
bool Adaptive_PWM_IsRunning(const Adaptive_PWM_t* pwm);

#endif // HAL_PWM_H
