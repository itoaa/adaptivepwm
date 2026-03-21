/**
 * @file freertos_tasks.h
 * @brief Task definitions for AdaptivePWM
 * 
 * Supports both FreeRTOS and bare-metal operation
 */

#ifndef FREERTOS_TASKS_H
#define FREERTOS_TASKS_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

// FreeRTOS includes - conditionally compiled
#ifdef USE_FREERTOS
  #include "FreeRTOS.h"
  #include "task.h"
  #include "semphr.h"
  #include "queue.h"
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

// Task priorities
#define TASK_PRIORITY_SAFETY       4  // Highest
#define TASK_PRIORITY_MEASURE      3
#define TASK_PRIORITY_CONTROL      2
#define TASK_PRIORITY_CLI          1  // Lowest

// Task stack sizes (in words)
#define STACK_SIZE_SAFETY         128
#define STACK_SIZE_MEASURE        256
#define STACK_SIZE_CONTROL        256
#define STACK_SIZE_CLI            512

// Task frequencies (Hz)
#define TASK_FREQ_SAFETY          100   // 100Hz = 10ms
#define TASK_FREQ_MEASURE         1000  // 1kHz = 1ms
#define TASK_FREQ_CONTROL         100   // 100Hz = 10ms
#define TASK_FREQ_CLI             50    // 50Hz = 20ms

typedef enum {
    TASK_STATE_RUNNING,
    TASK_STATE_PAUSED,
    TASK_STATE_ERROR
} TaskState_t;

typedef struct {
    TaskHandle_t safety_handle;
    TaskHandle_t measure_handle;
    TaskHandle_t control_handle;
    TaskHandle_t cli_handle;
    
    SemaphoreHandle_t adc_ready_sem;
    SemaphoreHandle_t pwm_ready_sem;
    SemaphoreHandle_t params_ready_sem;
    
    QueueHandle_t duty_queue;
    QueueHandle_t error_queue;
    
    volatile TaskState_t system_state;
    uint32_t active_task_count;
} TaskManager_t;

bool Tasks_Init(TaskManager_t* manager);
void Tasks_StartScheduler(void);
void Tasks_SuspendControl(TaskManager_t* manager);
void Tasks_ResumeControl(TaskManager_t* manager);
void Tasks_TriggerSafety(TaskManager_t* manager, uint32_t error_code);
uint32_t Tasks_GetStats(const TaskManager_t* manager, char* buffer, uint32_t buffer_size);

#endif // FREERTOS_TASKS_H
