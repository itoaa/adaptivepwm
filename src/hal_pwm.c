/**
 * @file hal_pwm.c
 * @brief PWM Hardware Abstraction Layer implementation
 */

#include "hal_pwm.h"
#include "config.h"
#include "error_handler.h"
#include "adaptive_assert.h"
#include <string.h>
#include <stdio.h>

// External error manager
extern ErrorManager_t error_manager;


// Convert nanoseconds to timer ticks (at 84MHz)
#define NS_TO_TICKS(ns) ((uint32_t)((ns) * 84 / 1000))

static void PWM_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    GPIO_InitStruct.Pin = PWM_GPIO_PIN_CH1 | PWM_GPIO_PIN_CH2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = PWM_GPIO_AF;
    HAL_GPIO_Init(PWM_GPIO_PORT, &GPIO_InitStruct);
    
    // Configure fault input (PA12 - TIM1_BKIN) for emergency stop
    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

bool Adaptive_PWM_Init(Adaptive_PWM_t* pwm)
{
    ADAPTIVE_ASSERT(pwm != NULL);
    
    if (pwm == NULL) {
        return false;
    }
    
    memset(pwm, 0, sizeof(Adaptive_PWM_t));
    
    __HAL_RCC_TIM1_CLK_ENABLE();
    
    // Configure timer
    pwm->htim.Instance = PWM_TIMER;
    pwm->htim.Init.Prescaler = 0;  // Full 84MHz clock
    pwm->htim.Init.CounterMode = TIM_COUNTERMODE_UP;
    pwm->htim.Init.Period = PWM_ARR_VALUE(PWM_FREQUENCY_HZ);
    pwm->htim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    pwm->htim.Init.RepetitionCounter = 0;
    
    if (HAL_TIM_PWM_Init(&pwm->htim) != HAL_OK) {
        DEBUG_PRINT("PWM: HAL_TIM_PWM_Init failed");
        return false;
    }
    
    // Configure PWM output
    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = pwm->htim.Init.Period / 2;  // 50% duty start
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    
    if (HAL_TIM_PWM_ConfigChannel(&pwm->htim, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) {
        DEBUG_PRINT("PWM: HAL_TIM_PWM_ConfigChannel failed");
        return false;
    }
    
    // Configure dead-time
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTime = {0};
    sBreakDeadTime.OffStateRunMode = TIM_OSSR_DISABLE;
    sBreakDeadTime.OffStateIDLEMode = TIM_OSSI_DISABLE;
    sBreakDeadTime.LockLevel = TIM_LOCKLEVEL_OFF;
    sBreakDeadTime.DeadTime = NS_TO_TICKS(PWM_DEAD_TIME_NS);
    sBreakDeadTime.BreakState = TIM_BREAK_ENABLE;
    sBreakDeadTime.BreakPolarity = TIM_BREAKPOLARITY_LOW;
    sBreakDeadTime.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
    
    if (HAL_TIMEx_ConfigBreakDeadTime(&pwm->htim, &sBreakDeadTime) != HAL_OK) {
        DEBUG_PRINT("PWM: HAL_TIMEx_ConfigBreakDeadTime failed");
        return false;
    }
    
    PWM_GPIO_Init();
    
    pwm->period = pwm->htim.Init.Period;
    pwm->frequency = PWM_FREQUENCY_HZ;
    pwm->current_duty = pwm->period / 2;
    pwm->is_running = false;
    
    DEBUG_PRINT("PWM: Initialized at %lu Hz", pwm->frequency);
    
    return true;
}

bool Adaptive_PWM_Start(Adaptive_PWM_t* pwm)
{
    ADAPTIVE_ASSERT(pwm != NULL);
    
    if (pwm == NULL || pwm->is_running) {
        return false;
    }
    
    if (HAL_TIM_PWM_Start(&pwm->htim, TIM_CHANNEL_1) != HAL_OK) {
        DEBUG_PRINT("PWM: HAL_TIM_PWM_Start failed");
        return false;
    }
    
    if (HAL_TIMEx_PWMN_Start(&pwm->htim, TIM_CHANNEL_1) != HAL_OK) {
        DEBUG_PRINT("PWM: HAL_TIMEx_PWMN_Start failed");
        HAL_TIM_PWM_Stop(&pwm->htim, TIM_CHANNEL_1);
        return false;
    }
    
    pwm->is_running = true;
    DEBUG_PRINT("PWM: Started");
    
    return true;
}

bool Adaptive_PWM_Stop(Adaptive_PWM_t* pwm)
{
    ADAPTIVE_ASSERT(pwm != NULL);
    
    if (pwm == NULL || !pwm->is_running) {
        return false;
    }
    
    HAL_TIM_PWM_Stop(&pwm->htim, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Stop(&pwm->htim, TIM_CHANNEL_1);
    
    pwm->is_running = false;
    pwm->current_duty = 0;
    
    DEBUG_PRINT("PWM: Stopped");
    
    return true;
}

bool Adaptive_PWM_SetDuty(Adaptive_PWM_t* pwm, float duty)
{
    ADAPTIVE_ASSERT(pwm != NULL);
    
    if (pwm == NULL || !pwm->is_running) {
        return false;
    }
    
    // Check hard limits first (safety critical)
    if (duty < PWM_HARD_MIN_DUTY) {
        Error_Report(&error_manager, ERR_PWM_FAULT, SEVERITY_WARNING,
                     "Duty below hardware minimum", (uint32_t)(duty * 1000));
        duty = PWM_HARD_MIN_DUTY;
    }
    
    if (duty > PWM_HARD_MAX_DUTY) {
        Error_Report(&error_manager, ERR_PWM_FAULT, SEVERITY_WARNING,
                     "Duty above hardware maximum", (uint32_t)(duty * 1000));
        duty = PWM_HARD_MAX_DUTY;
    }
    
    // Then apply soft limits
    if (duty < PWM_SOFT_MIN_DUTY) duty = PWM_SOFT_MIN_DUTY;
    if (duty > PWM_SOFT_MAX_DUTY) duty = PWM_SOFT_MAX_DUTY;
    
    uint32_t pulse = (uint32_t)(duty * pwm->period);
    
    __HAL_TIM_SET_COMPARE(&pwm->htim, TIM_CHANNEL_1, pulse);
    pwm->current_duty = (uint16_t)pulse;
    
    DEBUG_PRINT_EVERY_N(100, "PWM: Duty set to %.2f%%", duty * 100);
    
    return true;
}

float Adaptive_PWM_GetDuty(const Adaptive_PWM_t* pwm)
{
    ADAPTIVE_ASSERT(pwm != NULL);
    
    if (pwm == NULL || pwm->period == 0) {
        return 0.0f;
    }
    return (float)pwm->current_duty / pwm->period;
}

void Adaptive_PWM_EmergencyStop(Adaptive_PWM_t* pwm)
{
    if (pwm == NULL) {
        return;
    }
    
    // Immediate stop via break input
    HAL_TIM_GenerateEvent(&pwm->htim, TIM_EVENTSOURCE_BREAK);
    
    // Force outputs to inactive state
    __HAL_TIM_SET_COMPARE(&pwm->htim, TIM_CHANNEL_1, 0);
    
    HAL_TIM_PWM_Stop(&pwm->htim, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Stop(&pwm->htim, TIM_CHANNEL_1);
    
    pwm->is_running = false;
    pwm->current_duty = 0;
    
    DEBUG_PRINT("PWM: EMERGENCY STOP!");
}

uint32_t Adaptive_PWM_GetFrequency(const Adaptive_PWM_t* pwm)
{
    ADAPTIVE_ASSERT(pwm != NULL);
    
    if (pwm == NULL) {
        return 0;
    }
    return pwm->frequency;
}

bool Adaptive_PWM_IsRunning(const Adaptive_PWM_t* pwm)
{
    if (pwm == NULL) return false;
    return pwm->is_running;
}
