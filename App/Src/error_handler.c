/**
 * @file error_handler.c
 * @brief Error handler implementation with WD-refresh protection
 * 
 * SECURITY UPDATE 2026-04-16: PWM-ARCH-001
 * - Added FreeRTOS stack overflow hook (RTOS-002 MAJOR finding)
 * - Integrated with fault history logging
 */

#include "error_handler.h"
#include "hal_pwm.h"
#include "hal_watchdog.h"
#include "config.h"
#include "adaptive_assert.h"
#include <string.h>
#include <stdio.h>

// Include fault history for stack overflow logging
#ifdef USE_FREERTOS
  #include "fault_history.h"
#endif

extern Adaptive_PWM_t pwm_handle;

// Stats for debug
STATS_DECLARE(total_errors);
STATS_DECLARE(critical_errors);

void Error_Init(ErrorManager_t* manager)
{
    ADAPTIVE_ASSERT(manager != NULL);
    
    if (manager == NULL) return;
    
    memset(manager, 0, sizeof(ErrorManager_t));
    manager->system_fault = false;
}

void Error_Report(ErrorManager_t* manager, uint16_t code, ErrorSeverity_t severity, 
                  const char* message, uint32_t data)
{
    if (manager == NULL) return;
    
    STATS_INCREMENT(total_errors);
    
    // Log error
    ErrorEvent_t* event = &manager->events[manager->write_index];
    event->code = code;
    event->severity = severity;
    event->timestamp = HAL_GetTick();
    event->message = message;
    event->data = data;
    
    manager->write_index = (manager->write_index + 1) % ERROR_LOG_SIZE;
    if (manager->count < ERROR_LOG_SIZE) manager->count++;
    
    if (code < 16) {
        manager->error_count[code]++;
    }
    
    DEBUG_PRINT("ERROR [%u]: %s (code=0x%04X, data=%lu)", 
                severity, message ? message : "?", code, data);
    
    // Handle severity
    switch (severity) {
        case SEVERITY_INFO:
            // Just logged
            break;
            
        case SEVERITY_WARNING:
            // Could trigger notification
            break;
            
        case SEVERITY_ERROR:
            // Pause operation
            Adaptive_PWM_Stop(&pwm_handle);
            break;
            
        case SEVERITY_CRITICAL:
            STATS_INCREMENT(critical_errors);
            Error_Critical(manager, code, message);
            break;
    }
    
    // Refresh watchdog after error handling
    Adaptive_WDG_Refresh();
}

void Error_Critical(ErrorManager_t* manager, uint16_t code, const char* message)
{
    (void)code;  // Logged but not used in action
    (void)message;
    
    if (manager != NULL) {
        manager->system_fault = true;
    }
    
    DEBUG_PRINT("CRITICAL ERROR - System halt initiated");
    
    // 1. Emergency stop PWM
    Adaptive_PWM_EmergencyStop(&pwm_handle);
    
    // 2. Log to flash if possible (placeholder)
    // FlashLogger_Write(...)
    
    // 3. Refresh watchdog to prevent double-reset
    // This gives time for debug output
    for (int i = 0; i < 10; i++) {
        Adaptive_WDG_Refresh();
        HAL_Delay(100);  // Allow UART to flush
    }
    
    // 4. Safe GPIO state
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8 | GPIO_PIN_9, GPIO_PIN_RESET);
    
    // 5. Halt system - requires manual reset
    DEBUG_PRINT("System halted - manual reset required");
    while (1) {
        // Slow blink if LED available
        HAL_Delay(500);
    }
}

const ErrorEvent_t* Error_GetLast(const ErrorManager_t* manager)
{
    if (manager == NULL || manager->count == 0) return NULL;
    
    uint8_t last_index = (manager->write_index + ERROR_LOG_SIZE - 1) % ERROR_LOG_SIZE;
    return &manager->events[last_index];
}

uint32_t Error_GetCount(const ErrorManager_t* manager, uint16_t code)
{
    if (manager == NULL || code >= 16) return 0;
    return manager->error_count[code];
}

bool Error_IsFault(const ErrorManager_t* manager)
{
    return (manager != NULL) && manager->system_fault;
}

void Error_ClearFault(ErrorManager_t* manager)
{
    if (manager != NULL) {
        manager->system_fault = false;
    }
}

uint16_t Error_GetLog(const ErrorManager_t* manager, char* buffer, uint16_t size)
{
    if (manager == NULL || buffer == NULL || size == 0) return 0;
    
    uint16_t written = 0;
    written += snprintf(buffer + written, size - written, "Error Log:\r\n");
    
    for (uint8_t i = 0; i < manager->count; i++) {
        uint8_t idx = (manager->write_index + ERROR_LOG_SIZE - 1 - i) % ERROR_LOG_SIZE;
        const ErrorEvent_t* evt = &manager->events[idx];
        
        const char* sev_str = "?";
        switch (evt->severity) {
            case SEVERITY_INFO: sev_str = "INFO"; break;
            case SEVERITY_WARNING: sev_str = "WARN"; break;
            case SEVERITY_ERROR: sev_str = "ERR"; break;
            case SEVERITY_CRITICAL: sev_str = "CRIT"; break;
        }
        
        written += snprintf(buffer + written, size - written,
            "[%lu] %s 0x%04X: %s (data=%lu)\r\n",
            (unsigned long)evt->timestamp, sev_str, evt->code, 
            evt->message ? evt->message : "?", (unsigned long)evt->data);
        
        if (written >= size - 1) break;
    }
    
    return written;
}

/* ============================================================================
 * FREERTOS STACK OVERFLOW HOOK
 * ============================================================================
 * SECURITY FIX PWM-ARCH-001 (RTOS-002 MAJOR finding)
 * 
 * FreeRTOS requires this hook function when configCHECK_FOR_STACK_OVERFLOW
 * is enabled. It is called when a stack overflow is detected.
 * 
 * This function is called from the RTOS kernel (typically from the tick ISR
 * or context switch). Due to ISR context, we minimize operations and
 * defer complex handling to the fault history system which is ISR-safe.
 * ============================================================================ */

#ifdef USE_FREERTOS

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;  // Task handle available if needed
    
    // Log to fault history if available (ISR-safe)
    FaultHistory_Log(FAULT_TYPE_SOFTWARE_FAULT, FAULT_SEVERITY_FATAL,
                     ERR_STACK_OVERFLOW, 0);
    
    // Call critical error handler - this halts the system
    // We pass the task name as the message for debugging
    // Note: pcTaskName may be truncated to 16 chars by FreeRTOS
    extern ErrorManager_t error_manager;
    Error_Critical(&error_manager, ERR_STACK_OVERFLOW, 
                   pcTaskName ? pcTaskName : "unknown");
    
    // Should never reach here - Error_Critical halts
    while (1) {
        // Infinite loop as fail-safe
    }
}

#endif /* USE_FREERTOS */
