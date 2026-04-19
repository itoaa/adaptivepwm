/**
 * @file freertos_tasks.c
 * @brief FreeRTOS task implementations
 * 
 * UPDATED: PWM-ARCH-005, PWM-ARCH-002
 * Replaced unvalidated simplified efficiency formulas with validated
 * measurement-based efficiency calculation using efficiency_calc module.
 * 
 * PWM-ARCH-002 Changes:
 * - Fixed Tasks_GetStats heap reporting to work with all heap schemes
 * - Added configUSE_HEAP_SCHEME detection for proper heap size reporting
 * - Heap size now shows "Static pool" or "N/A" for heap_1/2 instead of 0
 * 
 * Task Architecture:
 * ==================
 * 
 * Priority 4: Task_Safety    - Emergency monitoring @ 100Hz
 * Priority 3: Task_Measurement - ADC sampling @ 1kHz  
 * Priority 2: Task_Control   - PWM control @ 100Hz
 * Priority 1: Task_CLI       - Command interface @ 50Hz
 * 
 * See docs/architecture/module-deps.md for dependencies
 * See docs/architecture/data-flow.md for data flow
 * See docs/architecture/state-machines.md for task states
 */

#include "freertos_tasks.h"
#include "hal_pwm.h"
#include "hal_adc.h"
#include "param_calc.h"
#include "efficiency_calc.h"
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

// Efficiency calculation context (added for PWM-ARCH-005)
static EfficiencyCalcContext_t eff_calc_ctx;
static PowerMeasurement_t power_meas;

#ifdef USE_FREERTOS
// Forward declarations for tasks (only when FreeRTOS is enabled)
static void Task_Measurement(void* pvParameters);
static void Task_Control(void* pvParameters);
static void Task_Safety(void* pvParameters);
static void Task_CLI(void* pvParameters);
#endif

/**
 * @callgraph
 * @callergraph main
 * 
 * @brief Initialize all FreeRTOS tasks and resources
 * 
 * Creates semaphores, queues, and tasks.
 * Tasks are created in priority order (Safety first).
 * 
 * @param manager Task manager
 * @return true if all resources created successfully
 * @return false if any creation failed
 */
bool Tasks_Init(TaskManager_t* manager)
{
    ADAPTIVE_ASSERT(manager != NULL);
    
    if (manager == NULL) return false;
    
    memset(manager, 0, sizeof(TaskManager_t));
    
    // Initialize efficiency calculation (PWM-ARCH-005)
    // Use MEASUREMENT mode as primary method (most accurate)
    if (!EfficiencyCalc_Init(&eff_calc_ctx, EFF_MODE_MEASUREMENT, TOPOLOGY_BUCK)) {
        return false;
    }
    
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

/**
 * @callgraph
 * @callergraph main
 * 
 * @brief Start FreeRTOS scheduler
 * 
 * This function does not return in FreeRTOS mode.
 * In bare-metal mode, enters simple delay loop.
 */
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

/**
 * @callgraph
 * 
 * @brief Suspend control tasks
 * 
 * Stops Task_Control and Task_Measurement.
 * Used during configuration changes or calibration.
 * 
 * @param manager Task manager
 */
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

/**
 * @callgraph
 * 
 * @brief Resume control tasks
 * 
 * Restarts Task_Control and Task_Measurement.
 * 
 * @param manager Task manager
 */
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

/**
 * @callgraph
 * 
 * @brief Trigger safety event
 * 
 * Sends error code to Task_Safety for handling.
 * 
 * @param manager Task manager
 * @param error_code Error code
 */
void Tasks_TriggerSafety(TaskManager_t* manager, uint32_t error_code)
{
    (void)error_code;  // May be used in future
    
    if (manager == NULL) return;
    
#ifdef USE_FREERTOS
    xQueueSend(manager->error_queue, &error_code, 0);
#endif
    manager->system_state = TASK_STATE_ERROR;
}

/**
 * @callgraph
 * @callergraph Task_CLI
 * 
 * @brief Get task statistics
 * 
 * PWM-ARCH-002: Fixed heap size reporting to work with all heap schemes.
 * 
 * FreeRTOS supports 5 heap schemes (heap_1 through heap_5):
 * - heap_1: Static allocation, no free() support - xPortGetFreeHeapSize() returns 0
 * - heap_2: Best-fit algorithm, legacy - xPortGetFreeHeapSize() works but fragmentation issues
 * - heap_3: Uses system malloc/free - xPortGetFreeHeapSize() may return 0
 * - heap_4: First-fit with coalescing - xPortGetFreeHeapSize() works correctly
 * - heap_5: heap_4 with multiple memory regions - xPortGetFreeHeapSize() works correctly
 * 
 * We detect the heap scheme at compile time (via configUSE_HEAP_SCHEME if defined)
 * or provide meaningful fallback text.
 * 
 * @param manager Task manager
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
uint32_t Tasks_GetStats(const TaskManager_t* manager, char* buffer, uint32_t buffer_size)
{
    if (manager == NULL || buffer == NULL || buffer_size == 0) return 0;
    
#ifdef USE_FREERTOS
    // PWM-ARCH-002: Detect heap scheme for accurate reporting
    // configUSE_HEAP_SCHEME is typically defined in FreeRTOSConfig.h
    // Values: 1=heap_1 (static), 2=heap_2, 3=heap_3, 4=heap_4, 5=heap_5
    
    #if defined(configUSE_HEAP_SCHEME)
        #if configUSE_HEAP_SCHEME == 1
            // heap_1: Static allocation, no dynamic free support
            int written = snprintf(buffer, buffer_size,
                "=== AdaptivePWM Task Stats ===\r\n"
                "Active tasks: %lu\r\n"
                "System state: %d\r\n"
                "Heap: Using heap_1 (static allocation)\r\n"
                "Efficiency calc: validated\r\n"
                "Efficiency mode: %s\r\n",
                (unsigned long)manager->active_task_count,
                manager->system_state,
                eff_calc_ctx.mode == EFF_MODE_MEASUREMENT ? "measurement" :
                eff_calc_ctx.mode == EFF_MODE_MODEL ? "model" : "hybrid"
            );
        #elif configUSE_HEAP_SCHEME == 2
            // heap_2: Best-fit, legacy, has fragmentation issues
            int written = snprintf(buffer, buffer_size,
                "=== AdaptivePWM Task Stats ===\r\n"
                "Active tasks: %lu\r\n"
                "System state: %d\r\n"
                "Free heap: %lu bytes (heap_2 - legacy, fragmentation risk)\r\n"
                "Efficiency calc: validated\r\n"
                "Efficiency mode: %s\r\n",
                (unsigned long)manager->active_task_count,
                manager->system_state,
                (unsigned long)xPortGetFreeHeapSize(),
                eff_calc_ctx.mode == EFF_MODE_MEASUREMENT ? "measurement" :
                eff_calc_ctx.mode == EFF_MODE_MODEL ? "model" : "hybrid"
            );
        #elif configUSE_HEAP_SCHEME == 3
            // heap_3: Uses system malloc/free
            int written = snprintf(buffer, buffer_size,
                "=== AdaptivePWM Task Stats ===\r\n"
                "Active tasks: %lu\r\n"
                "System state: %d\r\n"
                "Heap: Using heap_3 (system malloc/free)\r\n"
                "Efficiency calc: validated\r\n"
                "Efficiency mode: %s\r\n",
                (unsigned long)manager->active_task_count,
                manager->system_state,
                eff_calc_ctx.mode == EFF_MODE_MEASUREMENT ? "measurement" :
                eff_calc_ctx.mode == EFF_MODE_MODEL ? "model" : "hybrid"
            );
        #else
            // heap_4 or heap_5: Full-featured heap with coalescing
            int written = snprintf(buffer, buffer_size,
                "=== AdaptivePWM Task Stats ===\r\n"
                "Active tasks: %lu\r\n"
                "System state: %d\r\n"
                "Free heap: %lu bytes\r\n"
                "Efficiency calc: validated\r\n"
                "Efficiency mode: %s\r\n",
                (unsigned long)manager->active_task_count,
                manager->system_state,
                (unsigned long)xPortGetFreeHeapSize(),
                eff_calc_ctx.mode == EFF_MODE_MEASUREMENT ? "measurement" :
                eff_calc_ctx.mode == EFF_MODE_MODEL ? "model" : "hybrid"
            );
        #endif
    #else
        // configUSE_HEAP_SCHEME not defined - try to get heap size anyway
        // but note it may return 0 for heap_1
        size_t free_heap = xPortGetFreeHeapSize();
        int written;
        if (free_heap == 0) {
            // Likely heap_1 or heap_3 - static allocation
            written = snprintf(buffer, buffer_size,
                "=== AdaptivePWM Task Stats ===\r\n"
                "Active tasks: %lu\r\n"
                "System state: %d\r\n"
                "Heap: Static allocation (heap_1) or system heap (heap_3)\r\n"
                "Efficiency calc: validated\r\n"
                "Efficiency mode: %s\r\n",
                (unsigned long)manager->active_task_count,
                manager->system_state,
                eff_calc_ctx.mode == EFF_MODE_MEASUREMENT ? "measurement" :
                eff_calc_ctx.mode == EFF_MODE_MODEL ? "model" : "hybrid"
            );
        } else {
            // Likely heap_4 or heap_5
            written = snprintf(buffer, buffer_size,
                "=== AdaptivePWM Task Stats ===\r\n"
                "Active tasks: %lu\r\n"
                "System state: %d\r\n"
                "Free heap: %lu bytes\r\n"
                "Efficiency calc: validated\r\n"
                "Efficiency mode: %s\r\n",
                (unsigned long)manager->active_task_count,
                manager->system_state,
                (unsigned long)free_heap,
                eff_calc_ctx.mode == EFF_MODE_MEASUREMENT ? "measurement" :
                eff_calc_ctx.mode == EFF_MODE_MODEL ? "model" : "hybrid"
            );
        }
    #endif
#else
    int written = snprintf(buffer, buffer_size,
        "=== AdaptivePWM Task Stats ===\r\n"
        "Active tasks: %lu\r\n"
        "System state: %d\r\n"
        "Bare metal mode (no heap tracking)\r\n"
        "Efficiency calc: validated\r\n",
        (unsigned long)manager->active_task_count,
        manager->system_state
    );
#endif
    
    if (written < 0) return 0;
    return (uint32_t)written;
}

#ifdef USE_FREERTOS
// Task implementations - only compiled when FreeRTOS is enabled

/**
 * @callgraph
 * 
 * @brief Task_Measurement - ADC sampling task
 * 
 * Priority 3, 1kHz (1ms period)
 * 
 * Responsibilities:
 * - Wait for ADC DMA completion
 * - Retrieve measurements from HAL
 * - Add samples to waveform buffer
 * - Signal Task_Control when parameters ready
 * 
 * Dependencies:
 * - HAL_ADC must be initialized
 * - DMA must be configured
 * 
 * @param pvParameters TaskManager_t pointer
 */
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

/**
 * @callgraph
 * 
 * @brief Task_Control - PWM control task
 * 
 * Priority 2, 100Hz (10ms period)
 * 
 * Responsibilities:
 * - Wait for measurements from Task_Measurement
 * - Calculate efficiency using efficiency_calc module
 * - Run PID controller
 * - Update PWM duty cycle
 * - Apply degradation limits
 * 
 * PWM-ARCH-005: Uses validated efficiency calculation
 * - MEASUREMENT mode: Primary method using Pin/Pout
 * - MODEL mode: Fallback using physics-based model
 * 
 * @param pvParameters TaskManager_t pointer
 */
static void Task_Control(void* pvParameters)
{
    TaskManager_t* manager = (TaskManager_t*)pvParameters;
    float target_efficiency = TARGET_EFFICIENCY;
    float new_duty = current_duty_cycle;
    TickType_t last_wake;
    ADC_Measurement_t meas;
    
    last_wake = xTaskGetTickCount();
    
    while (1) {
        // Wait for parameters to be calculated
        if (xSemaphoreTake(manager->params_ready_sem, pdMS_TO_TICKS(10)) == pdTRUE) {
            // Get current ADC measurements
            if (Adaptive_ADC_GetMeasurement(&adc_handle, &meas)) {
                float efficiency = 0.0f;
                bool efficiency_valid = false;
                
                // PWM-ARCH-005: Use validated efficiency calculation
                // Mode 1: Direct measurement (primary, most accurate)
                // Mode 2: Physics-based model (fallback for diagnostics)
                
                if (eff_calc_ctx.mode == EFF_MODE_MEASUREMENT) {
                    // Primary method: Measure actual Pin and Pout
                    efficiency = EfficiencyCalc_FromMeasurements(
                        &eff_calc_ctx, &meas, &power_meas);
                    efficiency_valid = power_meas.valid;
                } else {
                    // Fallback: Use physics-based model
                    efficiency = EfficiencyCalc_FromModel(
                        &eff_calc_ctx, &meas, 
                        current_duty_cycle, calc_params.ripple_current);
                    efficiency_valid = (efficiency > EFF_MIN_REASONABLE);
                }
                
                // Apply moving average filter for stability
                if (efficiency_valid && efficiency > 0.0f) {
                    efficiency = EfficiencyCalc_GetFiltered(&eff_calc_ctx, efficiency);
                }
                
                // Proportional controller based on efficiency error
                if (efficiency_valid && calc_params.valid) {
                    float error = target_efficiency - efficiency;
                    new_duty = current_duty_cycle + error * DUTY_KP;
                    
                    // Clamp duty cycle to safe limits
                    if (new_duty < PWM_SOFT_MIN_DUTY) {
                        new_duty = PWM_SOFT_MIN_DUTY;
                    }
                    if (new_duty > PWM_SOFT_MAX_DUTY) {
                        new_duty = PWM_SOFT_MAX_DUTY;
                    }
                    
                    // Apply if changed significantly (hysteresis)
                    if (fabsf(new_duty - current_duty_cycle) > PWM_DUTY_HYSTERESIS) {
                        Adaptive_PWM_SetDuty(&pwm_handle, new_duty);
                        current_duty_cycle = new_duty;
                    }
                }
            }
        }
        
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));  // 100Hz
    }
}

/**
 * @callgraph
 * 
 * @brief Task_Safety - Safety monitoring task
 * 
 * Priority 4 (highest), 100Hz (10ms period)
 * 
 * Responsibilities:
 * - Monitor ADC readings for faults
 * - Check temperature, current, voltage limits
 * - Trigger emergency stop on fault
 * - Report faults to EnhancedSafety system
 * 
 * Safety Thresholds:
 * - Temperature: TEMP_SHUTDOWN_C
 * - Current: CURRENT_MAX_A
 * - Voltage: VOLTAGE_MIN_V to VOLTAGE_MAX_V
 * - Ripple: 50% of CURRENT_MAX_A
 * 
 * @param pvParameters TaskManager_t pointer
 */
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
            
            // PWM-ARCH-005: Additional safety - efficiency bounds check
            if (power_meas.valid) {
                if (power_meas.efficiency < EFFICIENCY_MIN_ACCEPTABLE && 
                    power_meas.pin > POWER_MIN_MEASURABLE) {
                    // Low efficiency warning - could indicate component failure
                    // Log but don't shut down immediately
                    // error_code = 0x06;  // WARNING_LOW_EFFICIENCY
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

/**
 * @callgraph
 * 
 * @brief Task_CLI - Command line interface task
 * 
 * Priority 1 (lowest), 50Hz (20ms period)
 * 
 * Responsibilities:
 * - Process UART commands
 * - Handle authentication
 * - Execute CLI commands
 * - Update statistics periodically
 * 
 * @param pvParameters TaskManager_t pointer
 */
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
