#ifndef ADAPTIVE_PWM_H
#define ADAPTIVE_PWM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

// Constants for safe operation
#define MAX_DUTY_CYCLE 0.95f
#define MIN_DUTY_CYCLE 0.05f
#define TARGET_EFFICIENCY 0.95f
#define ADC_BUFFER_SIZE 16

// Data structures
typedef struct {
    float L_mH;           // Inductance in millihenries
    float C_uF;           // Capacitance in microfarads
    float ESR_mOhm;       // Equivalent Series Resistance in milliohms
    float duty_cycle;     // Current duty cycle (0.0 - 1.0)
    float efficiency;     // Calculated efficiency (0.0 - 1.0)
    bool initialized;     // System initialization status
} AdaptivePWM_State;

typedef struct {
    ADC_HandleTypeDef* hadc;
    TIM_HandleTypeDef* htim;
    uint16_t adc_buffer[ADC_BUFFER_SIZE];
    uint32_t adc_sum;
    uint32_t last_measurement_time;
    uint32_t last_adjustment_time;
} AdaptivePWM_Context;

// Function prototypes
uint16_t read_adc_safely(ADC_HandleTypeDef* hadc);
bool measure_electrical_parameters(AdaptivePWM_Context* ctx, AdaptivePWM_State* state);
float calculate_efficiency(float inductance, float capacitance, float esr);
bool adjust_duty_cycle(AdaptivePWM_Context* ctx, AdaptivePWM_State* state, float target_efficiency);
bool init_adc(ADC_HandleTypeDef* hadc);
void control_loop(AdaptivePWM_Context* ctx, AdaptivePWM_State* state);
void error_handler(void);
bool system_init(AdaptivePWM_Context* ctx);
void adaptive_pwm_init(AdaptivePWM_Context* ctx, AdaptivePWM_State* state);

#ifdef __cplusplus
}
#endif

#endif // ADAPTIVE_PWM_H