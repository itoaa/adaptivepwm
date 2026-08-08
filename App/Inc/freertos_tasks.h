/**
 * @file freertos_tasks.h
 * @brief FreeRTOS task implementations for AdaptivePWM control system
 * 
 * Supports both FreeRTOS and bare-metal operation.
 * 
 * Task Architecture:
 * ```
 * ┌─────────────────────────────────────────────────────────────┐
 * │                      FreeRTOS Kernel                        │
 * ├─────────────────────────────────────────────────────────────┤
 * │ Priority 4 │ Task_Safety     │ 100Hz │ Emergency monitoring │
 * │ Priority 3 │ Task_Measurement│ 1kHz  │ ADC sampling         │
 * │ Priority 2 │ Task_Control    │ 100Hz │ PWM control loop     │
 * │ Priority 1 │ Task_CLI        │ 50Hz  │ Command interface    │
 * └─────────────────────────────────────────────────────────────┘
 * ```
 * 
 * Security Updates:
 * - PWM-ARCH-001: Increased stack sizes (RTOS-001 CRITICAL)
 * - PWM-ARCH-001: Added stack overflow detection (RTOS-002 MAJOR)
 * - PWM-ARCH-002: Fixed heap reporting for all heap schemes
 * - PWM-ARCH-005: Validated efficiency calculation
 *
 * Architecture:
 * - See docs/architecture/module-deps.md for task dependencies
 * - See docs/architecture/data-flow.md for data flow
 * - See docs/architecture/state-machines.md for task states
 */

#ifndef FREERTOS_TASKS_H
#define FREERTOS_TASKS_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "hal_uart.h"

// FreeRTOS includes - conditionally compiled
#ifdef USE_FREERTOS
  #include "FreeRTOS.h"
  #include "task.h"
  #include "semphr.h"
  #include "queue.h"
  
  // FreeRTOS Stack Overflow Detection Configuration
  // Method 2: Checks the stack pointer during context switch
  #ifndef configCHECK_FOR_STACK_OVERFLOW
    #define configCHECK_FOR_STACK_OVERFLOW  2
  #endif
  
#else
  // Bare-metal stub definitions
  typedef void* TaskHandle_t;
  typedef void* SemaphoreHandle_t;
  typedef void* QueueHandle_t;
  typedef uint32_t TickType_t;
  typedef uint32_t BaseType_t;
  #define pdMS_TO_TICKS(x) (x)
  #define pdTRUE 1
  #define pdFALSE 0
  #define pdPASS 1
  #define configMAX_PRIORITIES 5
  
  // vTaskDelayUntil is a statement, not an expression
  #define vTaskDelayUntil(pxPreviousWakeTime, xTimeIncrement) do { \
      (void)(pxPreviousWakeTime); \
      HAL_Delay(xTimeIncrement); \
  } while(0)
  
  #define xSemaphoreCreateBinary() ((SemaphoreHandle_t)1)
  #define xSemaphoreGive(xSemaphore) ((void)(xSemaphore), pdTRUE)
  #define xSemaphoreTake(xSemaphore, xBlockTime) ((void)(xSemaphore), (void)(xBlockTime), pdTRUE)
  #define xQueueCreate(uxQueueLength, uxItemSize) ((QueueHandle_t)1)
  #define xQueueSend(xQueue, pvItemToQueue, xTicksToWait) ((void)(xQueue), (void)(pvItemToQueue), (void)(xTicksToWait), pdTRUE)
  #define xTaskCreate(pvTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask) \
      ((void)(pvTaskCode), (void)(pcName), (void)(usStackDepth), (void)(pvParameters), (void)(uxPriority), (void)(pxCreatedTask), pdPASS)
  #define vTaskStartScheduler() do { while(1) { HAL_Delay(100); } } while(0)
  #define vTaskSuspend(xTaskHandle) ((void)(xTaskHandle))
  #define vTaskResume(xTaskHandle) ((void)(xTaskHandle))
  #define vTaskSuspendAll() 
  #define taskDISABLE_INTERRUPTS() __disable_irq()
  #define xTaskGetTickCount() HAL_GetTick()
  #define xTaskGetSchedulerState() 2
  #define xPortSysTickHandler() 
  #define xPortGetFreeHeapSize() 0
#endif

// Task priorities (higher number = higher priority)
#define TASK_PRIORITY_SAFETY       4  // Highest - emergency handling
#define TASK_PRIORITY_MEASURE      3  // High - ADC sampling
#define TASK_PRIORITY_CONTROL      2  // Medium - control loop
#define TASK_PRIORITY_CLI          1  // Lowest - user interface

// Task stack sizes (in words = 4 bytes each)
// SECURITY FIX PWM-ARCH-001: Increased from previous values (RTOS-001 CRITICAL)
// Previous values: Safety=128, Measure=256, Control=256
// These were too small for the actual call stack depth during exception handling
#define STACK_SIZE_SAFETY         192   // Was 128 - 50% increase for exception handling
#define STACK_SIZE_MEASURE        384   // Was 256 - 50% increase for ADC DMA callbacks
#define STACK_SIZE_CONTROL        384   // Was 256 - 50% increase for PID calculations
#define STACK_SIZE_CLI            512   // Unchanged - adequate for CLI command processing

// Task stack high water mark monitoring
#ifdef USE_FREERTOS
  #define TASKS_GET_STACK_HIGH_WATER_MARK(xTask) uxTaskGetStackHighWaterMark(xTask)
#else
  #define TASKS_GET_STACK_HIGH_WATER_MARK(xTask) 0
#endif

// Task frequencies (Hz)
#define TASK_FREQ_SAFETY          100   // 100Hz = 10ms period
#define TASK_FREQ_MEASURE         1000  // 1kHz = 1ms period
#define TASK_FREQ_CONTROL         100   // 100Hz = 10ms period
#define TASK_FREQ_CLI             50    // 50Hz = 20ms period

/**
 * @brief Task system states
 * 
 * Overall state of the task management system.
 */
typedef enum {
    TASK_STATE_RUNNING,  // All tasks running normally
    TASK_STATE_PAUSED,   // Control tasks suspended
    TASK_STATE_ERROR     // Error state - some tasks stopped
} TaskState_t;

/**
 * @brief Task manager context
 * 
 * Contains handles to all tasks and synchronization primitives.
 * 
 * Synchronization:
 * - adc_ready_sem: Signaled when ADC DMA completes
 * - pwm_ready_sem: Signaled when PWM update complete
 * - params_ready_sem: Signaled when measurements ready for control
 * 
 * Queues:
 * - duty_queue: New duty cycle values from CLI
 * - error_queue: Safety error codes
 */
typedef struct {
    TaskHandle_t safety_handle;    // Task_Safety handle
    TaskHandle_t measure_handle;   // Task_Measurement handle
    TaskHandle_t control_handle;   // Task_Control handle
    TaskHandle_t cli_handle;       // Task_CLI handle
    
    SemaphoreHandle_t adc_ready_sem;     // ADC DMA complete
    SemaphoreHandle_t pwm_ready_sem;     // PWM update complete
    SemaphoreHandle_t params_ready_sem;  // Parameters calculated
    
    QueueHandle_t duty_queue;      // Duty cycle commands
    QueueHandle_t error_queue;     // Error notifications
    
    volatile TaskState_t system_state;  // Overall system state
    uint32_t active_task_count;    // Number of active tasks
} TaskManager_t;

/**
 * @brief Initialize all tasks
 * 
 * Creates FreeRTOS tasks and synchronization primitives.
 * Must be called before starting scheduler.
 *
 * Task Creation Order:
 * 1. Create semaphores and queues
 * 2. Create Task_Safety (highest priority)
 * 3. Create Task_Measurement
 * 4. Create Task_Control
 * 5. Create Task_CLI (lowest priority)
 *
 * Dependencies:
 * - HAL initialized
 * - ADC initialized
 * - PWM initialized
 * - Efficiency calc initialized
 *
 * @callgraph
 * @callergraph
 * 
 * @param manager Task manager to initialize
 * @return true if all tasks created successfully
 * @return false if any creation failed
 */
bool Tasks_Init(TaskManager_t* manager);

/**
 * @brief Start runtime (FreeRTOS scheduler or bare-metal superloop)
 *
 * Never returns.
 */
void Tasks_StartScheduler(void);

/** Vout regulation setpoint (volts) */
float Tasks_GetVoutSetpoint(void);
void Tasks_SetVoutSetpoint(float v);

/** Soft safety latch */
bool Tasks_IsSafetyFault(void);
uint32_t Tasks_GetLastFaultCode(void);
void Tasks_ClearSafetyFault(void);

/** Non-blocking serial monitor (runs in Loop_CLI) */
void Tasks_StartMonitor(Adaptive_UART_t* uart, int duration_s);
bool Tasks_MonitorActive(void);

/**
 * @brief Suspend control tasks
 * 
 * Stops Task_Control and Task_Measurement.
 * Used during configuration changes or calibration.
 *
 * Called by:
 * - CLI calibration commands
 * - Safety system on fault
 *
 * @callgraph
 * @callergraph
 * 
 * @param manager Task manager
 */
void Tasks_SuspendControl(TaskManager_t* manager);

/**
 * @brief Resume control tasks
 * 
 * Restarts Task_Control and Task_Measurement.
 *
 * Called by:
 * - CLI after calibration complete
 * - Safety system after recovery
 *
 * @callgraph
 * @callergraph
 * 
 * @param manager Task manager
 */
void Tasks_ResumeControl(TaskManager_t* manager);

/**
 * @brief Trigger safety event
 * 
 * Sends error code to Task_Safety for immediate handling.
 *
 * Called by:
 * - Interrupt handlers
 * - CLI commands
 * - Task_Control on control errors
 *
 * @callgraph
 * @callergraph
 * 
 * @param manager Task manager
 * @param error_code Error code from error_handler.h
 */
void Tasks_TriggerSafety(TaskManager_t* manager, uint32_t error_code);

/**
 * @brief Get task statistics
 * 
 * Generates human-readable statistics string.
 *
 * Called by:
 * - Task_CLI (periodic stats update)
 * - CLI status command
 *
 * **PWM-ARCH-002**: Fixed heap reporting for all heap schemes:
 * - heap_1: Reports "Static pool" (no dynamic allocation)
 * - heap_2/4/5: Reports actual free bytes
 * - heap_3: Reports "System heap"
 *
 * @callgraph
 * @callergraph
 * 
 * @param manager Task manager
 * @param buffer Output buffer for statistics string
 * @param buffer_size Buffer size in bytes
 * @return Number of bytes written
 */
uint32_t Tasks_GetStats(const TaskManager_t* manager, char* buffer, uint32_t buffer_size);

/**
 * @brief Task_Safety - Safety monitoring task
 * 
 * Highest priority task (Priority 4).
 * Runs at 100Hz (10ms period).
 *
 * Responsibilities:
 * - Monitor ADC readings for faults
 * - Check temperature limits
 * - Check current/voltage limits
 * - Trigger emergency stop on fault
 * - Report faults to EnhancedSafety system
 *
 * Processing Flow:
 * ```
 * Get ADC measurements
 *     ↓
 * Check thresholds (temp, current, voltage)
 *     ↓
 * If fault: Emergency stop + halt system
 *     ↓
 * Check efficiency bounds
 *     ↓
 * Yield to lower priority tasks
 * ```
 *
 * Safety Thresholds:
 * - Temperature: TEMP_SHUTDOWN_C (from config.h)
 * - Current: CURRENT_MAX_A
 * - Voltage: VOLTAGE_MIN_V to VOLTAGE_MAX_V
 * - Ripple: 50% of CURRENT_MAX_A
 *
 * @note This task never suspends - continuous monitoring
 */
//static void Task_Safety(void* pvParameters);  // Implementation in .c file

/**
 * @brief Task_Measurement - ADC sampling task
 * 
 * Priority 3 task.
 * Runs at 1kHz (1ms period).
 *
 * Responsibilities:
 * - Wait for ADC DMA completion
 * - Retrieve ADC measurements
 * - Add samples to waveform buffer
 * - Signal Task_Control when parameters ready
 *
 * Processing Flow:
 * ```
 * Wait for ADC DMA (via HAL interrupt)
 *     ↓
 * Get measurements from HAL_ADC
 *     ↓
 * Add to WaveformBuffer (ParamCalc_AddSample)
 *     ↓
 * Signal params_ready_sem
 *     ↓
 * Wait for next period
 * ```
 *
 * Dependencies:
 * - HAL_ADC must be initialized and started
 * - DMA must be configured
 *
 * @note Triggers Task_Control via semaphore
 */
//static void Task_Measurement(void* pvParameters);  // Implementation in .c file

/**
 * @brief Task_Control - PWM control task
 * 
 * Priority 2 task.
 * Runs at 100Hz (10ms period).
 *
 * Responsibilities:
 * - Wait for measurements from Task_Measurement
 * - Calculate efficiency using efficiency_calc module
 * - Run PID controller
 * - Update PWM duty cycle
 * - Apply degradation limits from EnhancedSafety
 *
 * Processing Flow:
 * ```
 * Take params_ready_sem (wait for Task_Measurement)
 *     ↓
 * Get ADC measurements
 *     ↓
 * Calculate efficiency (EfficiencyCalc_FromMeasurements)
 *     ↓
 * Apply moving average filter
 *     ↓
 * PID calculation: duty += (target - efficiency) * Kp
 *     ↓
 * Clamp duty to safe limits
 *     ↓
 * Apply degradation limit (EnhancedSafety_GetEffectiveDutyLimit)
 *     ↓
 * Update PWM duty
 *     ↓
 * Yield to Task_Safety and Task_Measurement
 * ```
 *
 * **PWM-ARCH-005**: Efficiency calculation modes:
 * - MEASUREMENT: Primary method using Pin/Pout measurements
 * - MODEL: Fallback using physics-based model
 * - HYBRID: Combined approach (future)
 *
 * @note Efficiency target from TARGET_EFFICIENCY in config.h
 */
//static void Task_Control(void* pvParameters);  // Implementation in .c file

/**
 * @brief Task_CLI - Command line interface task
 * 
 * Priority 1 task (lowest).
 * Runs at 50Hz (20ms period).
 *
 * Responsibilities:
 * - Process UART commands
 * - Handle authentication
 * - Execute CLI commands
 * - Update statistics periodically
 *
 * Processing Flow:
 * ```
 * Check UART for commands
 *     ↓
 * If command: CLI_ProcessCommand
 *     ↓
 * Update statistics (optional)
 *     ↓
 * Wait for next period
 * ```
 *
 * Commands handled:
 * - login/logout
 * - set duty
 * - status
 * - calibration
 * - safety commands
 *
 * @note Lowest priority - yields to control and safety tasks
 */
//static void Task_CLI(void* pvParameters);  // Implementation in .c file

#endif // FREERTOS_TASKS_H
