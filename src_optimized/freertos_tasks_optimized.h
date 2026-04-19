/**
 * @file freertos_tasks_optimized.h
 * @brief Optimized FreeRTOS Task Definitions with Low Jitter
 * 
 * Optimizations:
 * - Cycle-accurate task timing
 * - Reduced context switching overhead
 * - Optimized semaphore usage
 * - Memory pool pre-allocation
 * 
 * @version 2.3.2-OPT
 * @date 2026-04-16
 */

#ifndef FREERTOS_TASKS_OPTIMIZED_H
#define FREERTOS_TASKS_OPTIMIZED_H

#include "config_optimized.h"
#include "performance_profiler.h"
#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

// FreeRTOS includes - conditionally compiled
#ifdef USE_FREERTOS
  #include "FreeRTOS.h"
  #include "task.h"
  #include "semphr.h"
  #include "queue.h"
  #include "timers.h"
#else
  // Bare-metal stubs
  typedef void* TaskHandle_t;
  typedef void* SemaphoreHandle_t;
  typedef void* QueueHandle_t;
  typedef void* TimerHandle_t;
  typedef uint32_t TickType_t;
  typedef uint32_t BaseType_t;
  #define pdMS_TO_TICKS(x) (x)
  #define pdTRUE 1
  #define pdFALSE 0
  #define pdPASS 1
  #define configMAX_PRIORITIES 5
  
  #define vTaskDelayUntil(pxPreviousWakeTime, xTimeIncrement) do { \
      *(pxPreviousWakeTime) += (xTimeIncrement); \
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
  #define uxTaskGetStackHighWaterMark(x) 0
#endif

// =============================================================================
// TASK CONFIGURATION - Optimized
// =============================================================================

// Task priorities (higher number = higher priority)
#define TASK_PRIORITY_SAFETY       4  // Highest - emergency response
#define TASK_PRIORITY_CONTROL      3  // High - control loop
#define TASK_PRIORITY_MEASURE      2  // Medium - measurement processing
#define TASK_PRIORITY_CLI          1  // Lowest - user interface

// Optimized stack sizes (measured minimums with headroom)
#define STACK_SIZE_SAFETY         128   // Minimal stack (simple logic)
#define STACK_SIZE_CONTROL        256   // Control task with float operations
#define STACK_SIZE_MEASURE        192   // Measurement with DMA processing
#define STACK_SIZE_CLI            512   // CLI needs more for printf

// Task cycle times
#define TASK_PERIOD_MEASURE_MS      1     // 1kHz measurement
#define TASK_PERIOD_CONTROL_MS      10    // 100Hz control
#define TASK_PERIOD_SAFETY_MS       10    // 100Hz safety
#define TASK_PERIOD_CLI_MS          20    // 50Hz CLI

// Memory pool sizes for zero-allocation operation
#define TASK_POOL_QUEUE_ITEMS       4
#define TASK_POOL_ERROR_ITEMS       4

// =============================================================================
// TASK MANAGER STRUCTURE
// =============================================================================

typedef enum {
    TASK_STATE_RUNNING,
    TASK_STATE_PAUSED,
    TASK_STATE_ERROR,
    TASK_STATE_SHUTDOWN
} TaskState_t;

typedef struct {
    // Task handles
    TaskHandle_t safety_handle;
    TaskHandle_t measure_handle;
    TaskHandle_t control_handle;
    TaskHandle_t cli_handle;
    
    // Synchronization primitives
    SemaphoreHandle_t adc_ready_sem;
    SemaphoreHandle_t pwm_ready_sem;
    SemaphoreHandle_t params_ready_sem;
    
    // Communication queues
    QueueHandle_t duty_queue;
    QueueHandle_t error_queue;
    
    // State
    volatile TaskState_t system_state;
    uint32_t active_task_count;
    
    // Performance tracking
    uint32_t task_overruns[TASK_ID_COUNT];
    uint32_t task_jitter[TASK_ID_COUNT];
    
} TaskManager_Opt_t;

// =============================================================================
// API FUNCTIONS
// =============================================================================

/**
 * @brief Initialize optimized task manager
 * @param manager Pointer to task manager
 * @return true on success, false on failure
 */
bool Tasks_Opt_Init(TaskManager_Opt_t* manager);

/**
 * @brief Start FreeRTOS scheduler
 */
void Tasks_Opt_StartScheduler(void);

/**
 * @brief Suspend control tasks (for safe reconfiguration)
 * @param manager Pointer to task manager
 */
void Tasks_Opt_SuspendControl(TaskManager_Opt_t* manager);

/**
 * @brief Resume control tasks
 * @param manager Pointer to task manager
 */
void Tasks_Opt_ResumeControl(TaskManager_Opt_t* manager);

/**
 * @brief Trigger safety shutdown
 * @param manager Pointer to task manager
 * @param error_code Error code for logging
 */
void Tasks_Opt_TriggerSafety(TaskManager_Opt_t* manager, uint32_t error_code);

/**
 * @brief Get formatted task statistics
 * @param manager Pointer to task manager
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of bytes written
 */
uint32_t Tasks_Opt_GetStats(TaskManager_Opt_t* manager, char* buffer, uint32_t buffer_size);

/**
 * @brief Get stack usage statistics
 * @param manager Pointer to task manager
 * @param task_stack_free Array to fill with free stack words
 * @return true if statistics retrieved
 */
bool Tasks_Opt_GetStackStats(TaskManager_Opt_t* manager, uint32_t* task_stack_free);

/**
 * @brief Check if any task has overrun its deadline
 * @param manager Pointer to task manager
 * @return true if any task has overrun
 */
bool Tasks_Opt_CheckOverruns(TaskManager_Opt_t* manager);

// =============================================================================
// TASK NOTIFICATION API (faster than semaphores)
// =============================================================================

#ifdef USE_FREERTOS

/**
 * @brief Notify task from ISR (faster than semaphore)
 * @param task_handle Task to notify
 * @param value Notification value
 * @return pdTRUE if successful
 */
static FORCE_INLINE BaseType_t Tasks_NotifyFromISR(TaskHandle_t task_handle, uint32_t value) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR(task_handle, value, eSetBits, &xHigherPriorityTaskWoken);
    return xHigherPriorityTaskWoken;
}

/**
 * @brief Wait for notification with timeout
 * @param value Pointer to receive notification value
 * @param timeout_ticks Timeout in ticks
 * @return true if notification received
 */
static FORCE_INLINE bool Tasks_WaitNotify(uint32_t* value, TickType_t timeout_ticks) {
    return xTaskNotifyWait(0, ULONG_MAX, value, timeout_ticks) == pdTRUE;
}

/**
 * @brief Clear notification bits
 * @param bits_to_clear Bits to clear
 */
static FORCE_INLINE void Tasks_ClearNotify(uint32_t bits_to_clear) {
    xTaskNotifyStateClear(NULL);
    (void)bits_to_clear;
}

#endif // USE_FREERTOS

// =============================================================================
// OPTIMIZED DELAY MACROS
// =============================================================================

// Precise delay using cycle counter (for sub-millisecond delays)
#define TASK_PRECISE_DELAY_US(us) do { \
    uint32_t start = PROFILER_GET_CYCLES(); \
    uint32_t cycles = PROFILER_US_TO_CYCLES(us); \
    while ((PROFILER_GET_CYCLES() - start) < cycles); \
} while(0)

// Delay until next period with jitter measurement
#define TASK_DELAY_UNTIL_MEASURED(pxPreviousWakeTime, xTimeIncrement, task_id) do { \
    uint32_t _start_cycles = Profiler_StartTiming(task_id); \
    vTaskDelayUntil(pxPreviousWakeTime, xTimeIncrement); \
    Profiler_EndTiming(task_id, _start_cycles); \
} while(0)

#endif // FREERTOS_TASKS_OPTIMIZED_H
