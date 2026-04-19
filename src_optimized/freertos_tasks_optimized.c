/**
 * @file freertos_tasks_optimized.c
 * @brief Optimized FreeRTOS Task Implementations
 * 
 * @version 2.3.2-OPT
 * @date 2026-04-16
 */

#include "freertos_tasks_optimized.h"
#include "hal_adc_optimized.h"
#include "pid_controller_optimized.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// External handles (would be linked from main)
extern Adaptive_ADC_Opt_t adc_handle_opt;
extern Adaptive_PWM_t pwm_handle;
extern PID_Controller_t pid_controller;
extern float current_duty_cycle;
extern CalculatedParams_t calc_params;
extern WaveformBuffer_t waveform_buffer;

// Forward declarations
#ifdef USE_FREERTOS
static void Task_Measurement_Opt(void* pvParameters);
static void Task_Control_Opt(void* pvParameters);
static void Task_Safety_Opt(void* pvParameters);
static void Task_CLI_Opt(void* pvParameters);
#endif

bool Tasks_Opt_Init(TaskManager_Opt_t* manager)
{
    if (manager == NULL) return false;
    
    memset(manager, 0, sizeof(TaskManager_Opt_t));
    
    // Create semaphores
    manager->adc_ready_sem = xSemaphoreCreateBinary();
    manager->pwm_ready_sem = xSemaphoreCreateBinary();
    manager->params_ready_sem = xSemaphoreCreateBinary();
    
    if (manager->adc_ready_sem == NULL || 
        manager->pwm_ready_sem == NULL || 
        manager->params_ready_sem == NULL) {
        return false;
    }
    
    // Create queues (pre-allocated, zero-allocation during runtime)
    manager->duty_queue = xQueueCreate(TASK_POOL_QUEUE_ITEMS, sizeof(float));
    manager->error_queue = xQueueCreate(TASK_POOL_ERROR_ITEMS, sizeof(uint32_t));
    
    if (manager->duty_queue == NULL || manager->error_queue == NULL) {
        return false;
    }
    
#ifdef USE_FREERTOS
    // Create tasks with optimized priorities
    if (xTaskCreate(Task_Safety_Opt, "Safety", STACK_SIZE_SAFETY, manager, 
                    TASK_PRIORITY_SAFETY, &manager->safety_handle) != pdPASS) {
        return false;
    }
    
    if (xTaskCreate(Task_Measurement_Opt, "Measure", STACK_SIZE_MEASURE, manager,
                    TASK_PRIORITY_MEASURE, &manager->measure_handle) != pdPASS) {
        return false;
    }
    
    if (xTaskCreate(Task_Control_Opt, "Control", STACK_SIZE_CONTROL, manager,
                    TASK_PRIORITY_CONTROL, &manager->control_handle) != pdPASS) {
        return false;
    }
    
    if (xTaskCreate(Task_CLI_Opt, "CLI", STACK_SIZE_CLI, manager,
                    TASK_PRIORITY_CLI, &manager->cli_handle) != pdPASS) {
        return false;
    }
    
    manager->active_task_count = 4;
#else
    manager->active_task_count = 0;
#endif
    
    manager->system_state = TASK_STATE_RUNNING;
    
    return true;
}

void Tasks_Opt_StartScheduler(void)
{
#ifdef USE_FREERTOS
    vTaskStartScheduler();
#else
    while (1) {
        HAL_Delay(100);
    }
#endif
}

void Tasks_Opt_SuspendControl(TaskManager_Opt_t* manager)
{
    if (manager == NULL) return;
    
#ifdef USE_FREERTOS
    vTaskSuspend(manager->control_handle);
    vTaskSuspend(manager->measure_handle);
#endif
    manager->active_task_count -= 2;
}

void Tasks_Opt_ResumeControl(TaskManager_Opt_t* manager)
{
    if (manager == NULL) return;
    
#ifdef USE_FREERTOS
    vTaskResume(manager->control_handle);
    vTaskResume(manager->measure_handle);
#endif
    manager->active_task_count += 2;
}

void Tasks_Opt_TriggerSafety(TaskManager_Opt_t* manager, uint32_t error_code)
{
    if (manager == NULL) return;
    
#ifdef USE_FREERTOS
    xQueueSend(manager->error_queue, &error_code, 0);
#endif
    manager->system_state = TASK_STATE_ERROR;
}

uint32_t Tasks_Opt_GetStats(TaskManager_Opt_t* manager, char* buffer, uint32_t buffer_size)
{
    if (manager == NULL || buffer == NULL || buffer_size == 0) return 0;
    
    int written = snprintf(buffer, buffer_size,
        "=== Optimized Task Stats ===\r\n"
        "Active tasks: %lu\r\n"
        "System state: %d\r\n"
        "Overruns: Meas=%lu, Ctrl=%lu, Safe=%lu\r\n",
        (unsigned long)manager->active_task_count,
        (int)manager->system_state,
        (unsigned long)manager->task_overruns[TASK_ID_MEASURE],
        (unsigned long)manager->task_overruns[TASK_ID_CONTROL],
        (unsigned long)manager->task_overruns[TASK_ID_SAFETY]
    );
    
    if (written < 0) return 0;
    return (uint32_t)written;
}

bool Tasks_Opt_GetStackStats(TaskManager_Opt_t* manager, uint32_t* task_stack_free)
{
    if (manager == NULL || task_stack_free == NULL) return false;
    
#ifdef USE_FREERTOS
    task_stack_free[0] = uxTaskGetStackHighWaterMark(manager->measure_handle);
    task_stack_free[1] = uxTaskGetStackHighWaterMark(manager->control_handle);
    task_stack_free[2] = uxTaskGetStackHighWaterMark(manager->safety_handle);
    task_stack_free[3] = uxTaskGetStackHighWaterMark(manager->cli_handle);
    return true;
#else
    (void)manager;
    memset(task_stack_free, 0, sizeof(uint32_t) * 4);
    return false;
#endif
}

bool Tasks_Opt_CheckOverruns(TaskManager_Opt_t* manager)
{
    if (manager == NULL) return false;
    
    for (int i = 0; i < TASK_ID_COUNT; i++) {
        if (manager->task_overruns[i] > 0) {
            return true;
        }
    }
    return false;
}

#ifdef USE_FREERTOS
// =============================================================================
// TASK IMPLEMENTATIONS - Optimized
// =============================================================================

static void Task_Measurement_Opt(void* pvParameters)
{
    TaskManager_Opt_t* manager = (TaskManager_Opt_t*)pvParameters;
    TickType_t last_wake;
    ADC_Measurement_t meas;
    
    last_wake = xTaskGetTickCount();
    
    while (1) {
        uint32_t start_cycles = Profiler_StartTiming(TASK_ID_MEASURE);
        
        // Process ADC buffer if ready
        if (ADC_Opt_IsReady(&adc_handle_opt)) {
            if (ADC_Opt_GetMeasurement(&adc_handle_opt, &meas)) {
                // Add to waveform buffer
                ParamCalc_AddSample(&waveform_buffer, &meas);
                
                // Signal control task
                xSemaphoreGive(manager->params_ready_sem);
            }
        }
        
        // Check for DMA half-complete (double-buffering)
        if (adc_handle_opt.half_complete) {
            adc_handle_opt.half_complete = false;
            // Process the appropriate buffer half
            ADC_Opt_ProcessBuffer(&adc_handle_opt, 
                                 adc_handle_opt.process_buffer == adc_handle_opt.dma_buffer);
        }
        
        Profiler_EndTiming(TASK_ID_MEASURE, start_cycles);
        
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TASK_PERIOD_MEASURE_MS));
    }
}

static void Task_Control_Opt(void* pvParameters)
{
    TaskManager_Opt_t* manager = (TaskManager_Opt_t*)pvParameters;
    TickType_t last_wake;
    float target_efficiency = TARGET_EFFICIENCY;
    
    last_wake = xTaskGetTickCount();
    
    while (1) {
        uint32_t start_cycles = Profiler_StartTiming(TASK_ID_CONTROL);
        
        // Wait for new parameters
        if (xSemaphoreTake(manager->params_ready_sem, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (calc_params.valid) {
                // Calculate efficiency
                float switching_loss = 0.01f * calc_params.inductance_mH * 
                                      current_duty_cycle * current_duty_cycle;
                float conduction_loss = calc_params.esr_mOhm / 1000.0f * 
                                       calc_params.ripple_current * calc_params.ripple_current;
                float efficiency = 1.0f - (switching_loss + conduction_loss);
                
                // Optimized PID computation
                float new_duty = PID_Compute(&pid_controller, target_efficiency, 
                                            efficiency, TASK_PERIOD_CONTROL_MS / 1000.0f);
                
                // Apply with ramping and hysteresis
                if (fabsf(new_duty - current_duty_cycle) > PWM_DUTY_HYSTERESIS) {
                    Adaptive_PWM_SetDuty(&pwm_handle, new_duty);
                    current_duty_cycle = new_duty;
                    
                    // Mark PWM update for latency tracking
                    PROFILE_PWM_UPDATE();
                }
            }
        }
        
        Profiler_EndTiming(TASK_ID_CONTROL, start_cycles);
        
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TASK_PERIOD_CONTROL_MS));
    }
}

static void Task_Safety_Opt(void* pvParameters)
{
    TaskManager_Opt_t* manager = (TaskManager_Opt_t*)pvParameters;
    TickType_t last_wake;
    ADC_Measurement_t meas;
    
    last_wake = xTaskGetTickCount();
    
    while (1) {
        uint32_t start_cycles = Profiler_StartTiming(TASK_ID_SAFETY);
        
        bool fault = false;
        uint32_t error_code = 0;
        
        // Get averaged measurements (more stable for safety)
        if (ADC_Opt_GetAveraged(&adc_handle_opt, &meas)) {
            // Temperature check
            if (meas.temperature > TEMP_SHUTDOWN_C) {
                error_code = 0x01;
                fault = true;
            }
            
            // Current check
            if (fabsf(meas.current) > CURRENT_MAX_A) {
                error_code = 0x02;
                fault = true;
            }
            
            // Voltage checks
            if (meas.vin > VOLTAGE_MAX_V || meas.vout > VOLTAGE_MAX_V) {
                error_code = 0x03;
                fault = true;
            }
            
            if (meas.vin < VOLTAGE_MIN_V || meas.vout < VOLTAGE_MIN_V) {
                error_code = 0x04;
                fault = true;
            }
        }
        
        if (fault) {
            Adaptive_PWM_EmergencyStop(&pwm_handle);
            Tasks_Opt_TriggerSafety(manager, error_code);
            
            // Halt system
            vTaskSuspendAll();
            taskDISABLE_INTERRUPTS();
            while (1);
        }
        
        Profiler_EndTiming(TASK_ID_SAFETY, start_cycles);
        
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TASK_PERIOD_SAFETY_MS));
    }
}

static void Task_CLI_Opt(void* pvParameters)
{
    TaskManager_Opt_t* manager = (TaskManager_Opt_t*)pvParameters;
    TickType_t last_wake;
    char buffer[256];
    
    last_wake = xTaskGetTickCount();
    
    while (1) {
        // Update stats periodically
        Tasks_Opt_GetStats(manager, buffer, sizeof(buffer));
        
        // Output to UART (placeholder)
        (void)buffer;
        
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TASK_PERIOD_CLI_MS));
    }
}

#endif // USE_FREERTOS
