/**
 * @file freertos_tasks.c
 * @brief FreeRTOS task implementations
 */

#include "freertos_tasks.h"
#include "hal_pwm.h"
#include "hal_adc.h"
#include "param_calc.h"
#include "config.h"
#include "adaptive_assert.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// External handles (defined in main)
extern Adaptive_PWM_t pwm_handle;
extern Adaptive_ADC_t adc_handle;
extern WaveformBuffer_t waveform_buffer;
extern CalculatedParams_t calc_params;
extern float current_duty_cycle;

#ifdef USE_FREERTOS
// Forward declarations for tasks (only when FreeRTOS is enabled)
static void Task_Measurement(void* pvParameters);
static void Task_Control(void* pvParameters);
static void Task_Safety(void* pvParameters);
static void Task_CLI(void* pvParameters);
#endif

bool Tasks_Init(TaskManager_t* manager)
{
    ADAPTIVE_ASSERT(manager != NULL);
    
    if (manager == NULL) return false;
    
    memset(manager, 0, sizeof(TaskManager_t));
    
    // Create semaphores
    manager->adc_ready_sem = xSemaphoreCreateBinary();
    manager->pwm_ready_sem = xSemaphoreCreateBinary();
    manager->params_ready_sem = xSemaphoreCreateBinary();
    
    if (manager->adc_ready_sem == NULL || 
        manager->pwm_ready_sem == NULL || 
        manager->params_ready_sem == NULL) {
        return false;
    }
    
    // Create queues
    manager->duty_queue = xQueueCreate(4, sizeof(float));
    manager->error_queue = xQueueCreate(4, sizeof(uint32_t));
    
    if (manager->duty_queue == NULL || manager->error_queue == NULL) {
        return false;
    }
    
#ifdef USE_FREERTOS
    // Create tasks only when FreeRTOS is enabled
    if (xTaskCreate(Task_Safety, "Safety", STACK_SIZE_SAFETY, manager, 
                    TASK_PRIORITY_SAFETY, &manager->safety_handle) != pdPASS) return false;
    
    if (xTaskCreate(Task_Measurement, "Measure", STACK_SIZE_MEASURE, manager,
                    TASK_PRIORITY_MEASURE, &manager->measure_handle) != pdPASS) return false;
    
    if (xTaskCreate(Task_Control, "Control", STACK_SIZE_CONTROL, manager,
                    TASK_PRIORITY_CONTROL, &manager->control_handle) != pdPASS) return false;
    
    if (xTaskCreate(Task_CLI, "CLI", STACK_SIZE_CLI, manager,
                    TASK_PRIORITY_CLI, &manager->cli_handle) != pdPASS) return false;
#endif
    
    manager->system_state = TASK_STATE_RUNNING;
    manager->active_task_count = 4;
    
    return true;
}

void Tasks_StartScheduler(void)
{
#ifdef USE_FREERTOS
    vTaskStartScheduler();
#else
    // Bare metal fallback - simple loop
    while(1) {
        HAL_Delay(100);
    }
#endif
}

void Tasks_SuspendControl(TaskManager_t* manager)
{
    ADAPTIVE_ASSERT(manager != NULL);
    
    if (manager == NULL) return;
    
#ifdef USE_FREERTOS
    vTaskSuspend(manager->control_handle);
    vTaskSuspend(manager->measure_handle);
#endif
    manager->active_task_count -= 2;
}

void Tasks_ResumeControl(TaskManager_t* manager)
{
    ADAPTIVE_ASSERT(manager != NULL);
    
    if (manager == NULL) return;
    
#ifdef USE_FREERTOS
    vTaskResume(manager->control_handle);
    vTaskResume(manager->measure_handle);
#endif
    manager->active_task_count += 2;
}

void Tasks_TriggerSafety(TaskManager_t* manager, uint32_t error_code)
{
    (void)error_code;  // May be used in future
    
    if (manager == NULL) return;
    
#ifdef USE_FREERTOS
    xQueueSend(manager->error_queue, &error_code, 0);
#endif
    manager->system_state = TASK_STATE_ERROR;
}

uint32_t Tasks_GetStats(const TaskManager_t* manager, char* buffer, uint32_t buffer_size)
{
    if (manager == NULL || buffer == NULL || buffer_size == 0) return 0;
    
#ifdef USE_FREERTOS
    int written = snprintf(buffer, buffer_size,
        "=== AdaptivePWM Task Stats ===\r\n"
        "Active tasks: %lu\r\n"
        "System state: %d\r\n"
        "Free heap: %lu bytes\r\n",
        (unsigned long)manager->active_task_count,
        manager->system_state,
        (unsigned long)xPortGetFreeHeapSize()
    );
#else
    int written = snprintf(buffer, buffer_size,
        "=== AdaptivePWM Task Stats ===\r\n"
        "Active tasks: %lu\r\n"
        "System state: %d\r\n"
        "Bare metal mode (no heap)\r\n",
        (unsigned long)manager->active_task_count,
        manager->system_state
    );
#endif
    
    if (written < 0) return 0;
    return (uint32_t)written;
}

#ifdef USE_FREERTOS
// Task implementations - only compiled when FreeRTOS is enabled

static void Task_Measurement(void* pvParameters)
{
    TaskManager_t* manager = (TaskManager_t*)pvParameters;
    ADC_Measurement_t meas;
    TickType_t last_wake;
    
    last_wake = xTaskGetTickCount();
    
    while (1) {
        // Wait for ADC DMA completion
        if (Adaptive_ADC_IsReady(&adc_handle)) {
            if (Adaptive_ADC_GetMeasurement(&adc_handle, &meas)) {
                // Add to waveform buffer for parameter calculation
                ParamCalc_AddSample(&waveform_buffer, &meas);
                
                // Signal parameter calculation ready
                xSemaphoreGive(manager->params_ready_sem);
            }
        }
        
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1));  // 1kHz
    }
}

static void Task_Control(void* pvParameters)
{
    TaskManager_t* manager = (TaskManager_t*)pvParameters;
    float target_efficiency = TARGET_EFFICIENCY;
    float new_duty = current_duty_cycle;
    TickType_t last_wake;
    
    last_wake = xTaskGetTickCount();
    
    while (1) {
        // Wait for parameters to be calculated
        if (xSemaphoreTake(manager->params_ready_sem, pdMS_TO_TICKS(10)) == pdTRUE) {
            // Calculate new duty cycle based on efficiency
            if (calc_params.valid) {
                // Calculate efficiency from losses
                float switching_loss = 0.01f * calc_params.inductance_mH * 
                                      current_duty_cycle * current_duty_cycle;
                float conduction_loss = calc_params.esr_mOhm / 1000.0f * 
                                       calc_params.ripple_current * calc_params.ripple_current;
                float efficiency = 1.0f - (switching_loss + conduction_loss);
                
                // Proportional controller
                float error = target_efficiency - efficiency;
                new_duty = current_duty_cycle + error * DUTY_KP;
                
                // Clamp duty cycle
                if (new_duty < PWM_SOFT_MIN_DUTY) new_duty = PWM_SOFT_MIN_DUTY;
                if (new_duty > PWM_SOFT_MAX_DUTY) new_duty = PWM_SOFT_MAX_DUTY;
                
                // Apply if changed significantly
                if (fabsf(new_duty - current_duty_cycle) > 0.001f) {
                    Adaptive_PWM_SetDuty(&pwm_handle, new_duty);
                    current_duty_cycle = new_duty;
                }
            }
        }
        
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));  // 100Hz
    }
}

static void Task_Safety(void* pvParameters)
{
    TaskManager_t* manager = (TaskManager_t*)pvParameters;
    ADC_Measurement_t meas;
    uint32_t error_code = 0;
    TickType_t last_wake;
    
    last_wake = xTaskGetTickCount();
    
    while (1) {
        bool fault = false;
        
        // Get latest measurements
        if (Adaptive_ADC_GetAveraged(&adc_handle, &meas)) {
            // Temperature check
            if (meas.temperature > TEMP_SHUTDOWN_C) {
                error_code = 0x01;  // ERROR_OVER_TEMP
                fault = true;
            }
            
            // Current check
            if (fabsf(meas.current) > CURRENT_MAX_A) {
                error_code = 0x02;  // ERROR_OVER_CURRENT
                fault = true;
            }
            
            // Voltage checks
            if (meas.vin > VOLTAGE_MAX_V || meas.vout > VOLTAGE_MAX_V) {
                error_code = 0x03;  // ERROR_OVER_VOLTAGE
                fault = true;
            }
            
            if (meas.vin < VOLTAGE_MIN_V || meas.vout < VOLTAGE_MIN_V) {
                error_code = 0x04;  // ERROR_UNDER_VOLTAGE
                fault = true;
            }
            
            // Parameter validation
            if (calc_params.valid) {
                if (calc_params.ripple_current > CURRENT_MAX_A * 0.5f) {
                    error_code = 0x05;  // ERROR_INVALID_PARAMS
                    fault = true;
                }
            }
        }
        
        if (fault) {
            // Emergency stop
            Adaptive_PWM_EmergencyStop(&pwm_handle);
            Tasks_TriggerSafety(manager, error_code);
            
            // Log error and halt
            vTaskSuspendAll();
            taskDISABLE_INTERRUPTS();
            while (1);  // System halt - requires reset
        }
        
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));  // 100Hz
    }
}

static void Task_CLI(void* pvParameters)
{
    TaskManager_t* manager = (TaskManager_t*)pvParameters;
    TickType_t last_wake;
    char buffer[256];
    
    last_wake = xTaskGetTickCount();
    
    while (1) {
        // Process CLI commands (placeholder for actual UART implementation)
        (void)manager;
        (void)buffer;
        
        // Update stats periodically
        Tasks_GetStats(manager, buffer, sizeof(buffer));
        
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(20));  // 50Hz
    }
}

#endif // USE_FREERTOS
