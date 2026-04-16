# AdaptivePWM Design Update

**Document ID:** PWM-DESIGN-UPDATE  
**Version:** 3.0 (Proposed)  
**Date:** 2026-04-16  
**Status:** Draft / Proposed Architecture  
**Classification:** Internal Technical Design

---

## 1. Design Philosophy

### 1.1 Core Principles

This design update addresses the architectural issues identified in the review while maintaining the strengths of the current system:

1. **Dependency Injection** - Replace global state with context-based dependency injection
2. **Event-Driven Safety** - Decouple safety modules via event dispatcher
3. **Layer Isolation** - Enforce strict boundaries between HAL, Control, Safety, and Application layers
4. **Async Operations** - Eliminate blocking operations in critical paths
5. **Testability First** - Design for unit testing with mock HAL support

### 1.2 Architecture Vision

```
┌─────────────────────────────────────────────────────────────────┐
│                      APPLICATION LAYER                           │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐  │
│  │ CLI Handler │  │    Main     │  │     Calibration         │  │
│  │  (RBAC)     │  │    Loop     │  │       Manager         │  │
│  └──────┬──────┘  └──────┬──────┘  └────────────┬────────────┘  │
└─────────┼────────────────┼──────────────────────┼───────────────┘
          │                │                      │
          │     ┌──────────▼──────────────────────▼──────┐        │
          │     │         SERVICE LAYER                  │        │
          │     │  ┌─────────────┐    ┌───────────────┐   │        │
          │     │  │   Event     │    │  Parameter    │   │        │
          │     │  │ Dispatcher  │    │  Calculator   │   │        │
          │     │  └──────┬──────┘    └───────┬───────┘   │        │
          │     └─────────┼───────────────────┼───────────┘        │
          │               │                   │                      │
          │     ┌─────────▼───────────────────▼──────────┐         │
          │     │         CONTROL LAYER                  │         │
          │     │  ┌─────────────┐    ┌───────────────┐   │         │
          │     │  │  FreeRTOS   │    │     PID       │   │         │
          │     │  │   Scheduler │    │  Controller   │   │         │
          │     │  └──────┬──────┘    └───────┬───────┘   │         │
          │     └─────────┼───────────────────┼───────────┘         │
          │               │                   │                       │
          │     ┌─────────▼───────────────────▼──────────┐          │
          │     │         SAFETY LAYER                   │          │
          │     │  ┌─────────────┐    ┌───────────────┐   │          │
          │     │  │   Enhanced  │    │  Fault Mgmt   │   │          │
          │     │  │   Safety    │    │  (Event-based)│   │          │
          │     │  └──────┬──────┘    └───────┬───────┘   │          │
          │     └─────────┼───────────────────┼───────────┘          │
          │               │                   │                        │
          │     ┌─────────▼───────────────────▼──────────┐           │
          │     │      HARDWARE ABSTRACTION             │           │
          │     │  ┌─────────┐ ┌─────────┐ ┌─────────┐   │           │
          │     │  │  PWM    │ │  ADC    │ │  UART   │   │           │
          │     │  │ Driver  │ │ Driver  │ │ Driver  │   │           │
          │     │  └────┬────┘ └────┬────┘ └────┬────┘   │           │
          │     └───────┼──────────┼──────────┼──────────┘           │
          │             │          │          │                        │
          └─────────────│──────────│──────────│────────────────────────┘
                        │          │          │
        ┌───────────────▼──────────▼──────────▼───────────────────────┐
        │              PLATFORM HAL (STM32Cube)                        │
        │         (Replaceable for porting to other platforms)        │
        └──────────────────────────────────────────────────────────────┘
```

---

## 2. System Context Architecture

### 2.1 System Context Structure

Replaces global variables with a single context pointer:

```c
// system_context.h
#ifndef SYSTEM_CONTEXT_H
#define SYSTEM_CONTEXT_H

#include <stdint.h>
#include <stdbool.h>

// Forward declarations (opaque pointers for loose coupling)
typedef struct Adaptive_PWM_t Adaptive_PWM_t;
typedef struct Adaptive_ADC_t Adaptive_ADC_t;
typedef struct Adaptive_UART_t Adaptive_UART_t;
typedef struct TaskManager_t TaskManager_t;
typedef struct ErrorManager_t ErrorManager_t;
typedef struct EnhancedSafetyManager_t EnhancedSafetyManager_t;
typedef struct TempMonitor_t TempMonitor_t;
typedef struct FlashLogger_t FlashLogger_t;
typedef struct EventDispatcher_t EventDispatcher_t;
typedef struct ParamCalculator_t ParamCalculator_t;

/**
 * @brief System context containing all subsystem handles
 * 
 * This structure is the single source of truth for system state.
 * All modules receive a pointer to this context during initialization.
 * 
 * @security This structure contains pointers to all subsystems.
 *           Protect from tampering in production environments.
 */
typedef struct {
    // Version info
    const char* version_string;
    uint32_t startup_timestamp;
    
    // HAL Layer (owned by HAL)
    Adaptive_PWM_t* pwm;
    Adaptive_ADC_t* adc;
    Adaptive_UART_t* uart;
    
    // Control Layer
    TaskManager_t* tasks;
    ParamCalculator_t* param_calc;
    
    // Safety Layer
    EnhancedSafetyManager_t* safety;
    TempMonitor_t* temp_monitor;
    ErrorManager_t* error_manager;
    
    // Service Layer
    FlashLogger_t* flash_logger;
    EventDispatcher_t* event_dispatcher;
    
    // System state
    volatile bool system_running;
    volatile bool emergency_stop;
    
    // Statistics
    uint32_t cycle_count;
    uint32_t error_count;
    
} SystemContext_t;

/**
 * @brief Initialize the system context
 * 
 * Allocates and initializes all subsystems.
 * Must be called before any other system function.
 * 
 * @param ctx Pointer to context structure (will be filled)
 * @return true if initialization successful
 * @pre ctx != NULL
 * @post All subsystem pointers are valid or NULL on failure
 */
bool SystemContext_Init(SystemContext_t* ctx);

/**
 * @brief Deinitialize system context
 * 
 * Safely shuts down all subsystems in reverse dependency order.
 * 
 * @param ctx System context
 * @return true if shutdown successful
 */
bool SystemContext_Deinit(SystemContext_t* ctx);

/**
 * @brief Validate context integrity
 * 
 * Checks that all required subsystems are initialized.
 * Used internally and for debugging.
 * 
 * @param ctx System context
 * @return true if context is valid
 */
bool SystemContext_Validate(const SystemContext_t* ctx);

/**
 * @brief Get global context (for legacy compatibility during migration)
 * 
 * @warning Deprecated - use dependency injection instead
 * @return Pointer to system context or NULL if not initialized
 */
SystemContext_t* SystemContext_Get(void);

#endif // SYSTEM_CONTEXT_H
```

### 2.2 Context Usage Pattern

```c
// Example: PWM driver with context
// hal_pwm.h
#ifndef HAL_PWM_H
#define HAL_PWM_H

#include "system_context.h"

typedef struct {
    // ... implementation details
} Adaptive_PWM_t;

/**
 * @brief Initialize PWM subsystem
 * @param pwm PWM handle (allocated by caller)
 * @param ctx System context (for dependency access)
 * @return true on success
 */
bool Adaptive_PWM_Init(Adaptive_PWM_t* pwm, SystemContext_t* ctx);

/**
 * @brief Set duty cycle with safety checks
 * @param pwm PWM handle
 * @param duty Duty cycle (0.0-1.0)
 * @return true on success
 * @note Queries safety system through context for limits
 */
bool Adaptive_PWM_SetDuty(Adaptive_PWM_t* pwm, float duty);

#endif // HAL_PWM_H

// hal_pwm.c
#include "hal_pwm.h"
#include "config_pwm.h"

bool Adaptive_PWM_Init(Adaptive_PWM_t* pwm, SystemContext_t* ctx) {
    // Validate inputs
    if (!pwm || !SystemContext_Validate(ctx)) {
        return false;
    }
    
    // Store context for later use
    pwm->system = ctx;
    
    // Initialize hardware...
    
    return true;
}

bool Adaptive_PWM_SetDuty(Adaptive_PWM_t* pwm, float duty) {
    if (!pwm || !pwm->system) return false;
    
    // Apply safety limits from safety system
    float max_duty = EnhancedSafety_GetEffectiveDutyLimit(
        pwm->system->safety, duty);
    
    if (duty > max_duty) {
        duty = max_duty;
    }
    
    // Update hardware...
    
    return true;
}
```

---

## 3. Event-Driven Safety Architecture

### 3.1 Event Dispatcher

Replaces direct function calls with decoupled events:

```c
// event_dispatcher.h
#ifndef EVENT_DISPATCHER_H
#define EVENT_DISPATCHER_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_EVENT_HANDLERS 16
#define MAX_QUEUED_EVENTS 32

/**
 * @brief Event types for system-wide communication
 */
typedef enum {
    // Safety events
    EVENT_FAULT_DETECTED = 0x100,
    EVENT_FAULT_CLEARED,
    EVENT_EMERGENCY_STOP,
    EVENT_EMERGENCY_CLEARED,
    EVENT_DEGRADATION_CHANGED,
    
    // Hardware events
    EVENT_ADC_SAMPLE_READY,
    EVENT_PWM_CYCLE_COMPLETE,
    EVENT_TEMP_WARNING,
    EVENT_TEMP_CRITICAL,
    
    // System events
    EVENT_CALIBRATION_COMPLETE,
    EVENT_LOG_ENTRY_WRITTEN,
    EVENT_CLI_COMMAND_RECEIVED,
    
    // User-defined events start here
    EVENT_USER_BASE = 0x1000
} EventType_t;

/**
 * @brief Event priority levels
 */
typedef enum {
    EVENT_PRIORITY_CRITICAL = 0,  // Process immediately
    EVENT_PRIORITY_HIGH,          // Process next
    EVENT_PRIORITY_NORMAL,        // Standard processing
    EVENT_PRIORITY_LOW            // Background processing
} EventPriority_t;

/**
 * @brief Event data structure
 */
typedef struct {
    EventType_t type;
    EventPriority_t priority;
    uint32_t timestamp;
    uint32_t source_id;      // Module that generated event
    
    // Event payload (union for type safety)
    union {
        // Fault event data
        struct {
            uint16_t fault_code;
            uint32_t context;
            float severity;    // 0.0-1.0
        } fault;
        
        // ADC event data
        struct {
            float voltage;
            float current;
            float temperature;
        } adc;
        
        // Generic data
        uint32_t data[4];
    } payload;
} Event_t;

/**
 * @brief Event handler function type
 */
typedef void (*EventHandler_t)(const Event_t* event, void* user_data);

/**
 * @brief Event filter function type
 */
typedef bool (*EventFilter_t)(const Event_t* event);

/**
 * @brief Event dispatcher handle
 */
typedef struct {
    struct {
        EventHandler_t handler;
        EventFilter_t filter;
        void* user_data;
        uint32_t mask;        // Which event types to receive
    } handlers[MAX_EVENT_HANDLERS];
    
    Event_t queue[MAX_QUEUED_EVENTS];
    volatile uint8_t head;
    volatile uint8_t tail;
    volatile uint8_t count;
    
    bool initialized;
} EventDispatcher_t;

// API functions
bool EventDispatcher_Init(EventDispatcher_t* dispatcher);
bool EventDispatcher_Deinit(EventDispatcher_t* dispatcher);

bool EventDispatcher_Register(
    EventDispatcher_t* dispatcher,
    EventType_t type_mask,    // OR of EVENT_TYPE_* to receive
    EventFilter_t filter,     // Optional filter (NULL = receive all)
    EventHandler_t handler,
    void* user_data
);

bool EventDispatcher_Unregister(
    EventDispatcher_t* dispatcher,
    EventHandler_t handler
);

bool EventDispatcher_Post(
    EventDispatcher_t* dispatcher,
    const Event_t* event
);

bool EventDispatcher_PostISR(
    EventDispatcher_t* dispatcher,
    const Event_t* event
);

void EventDispatcher_Process(EventDispatcher_t* dispatcher);

uint32_t EventDispatcher_GetQueueDepth(const EventDispatcher_t* dispatcher);

#endif // EVENT_DISPATCHER_H
```

### 3.2 Event Usage Example

```c
// Safety module subscribes to faults
void EnhancedSafety_Init(EventDispatcher_t* dispatcher) {
    // Receive all fault events
    EventDispatcher_Register(dispatcher, 
        EVENT_FAULT_DETECTED | EVENT_EMERGENCY_STOP,
        NULL,  // No filter
        Safety_FaultHandler,
        safety_manager);
}

static void Safety_FaultHandler(const Event_t* event, void* user_data) {
    EnhancedSafetyManager_t* safety = user_data;
    
    switch (event->type) {
        case EVENT_FAULT_DETECTED:
            // Log fault, check for degradation
            EnhancedSafety_HandleFault(safety, 
                event->payload.fault.fault_code,
                event->payload.fault.severity);
            break;
            
        case EVENT_EMERGENCY_STOP:
            // Immediate shutdown
            EnhancedSafety_EmergencyStop(safety, "External request");
            break;
            
        default:
            break;
    }
}

// Fault detection posts events
void CurrentProtection_Check(SystemContext_t* ctx) {
    float current = ADC_GetCurrent(ctx->adc);
    
    if (current > CURRENT_MAX_A) {
        Event_t event = {
            .type = EVENT_FAULT_DETECTED,
            .priority = EVENT_PRIORITY_CRITICAL,
            .timestamp = HAL_GetTick(),
            .source_id = MODULE_CURRENT_PROTECTION,
            .payload.fault = {
                .fault_code = ERR_OVER_CURRENT,
                .context = (uint32_t)(current * 1000),
                .severity = 0.9f
            }
        };
        
        EventDispatcher_Post(ctx->event_dispatcher, &event);
    }
}
```

---

## 4. Async Flash Logging

### 4.1 Async Logger Design

```c
// flash_logger_async.h
#ifndef FLASH_LOGGER_ASYNC_H
#define FLASH_LOGGER_ASYNC_H

#include "system_context.h"
#include "flash_logger.h"

#define FLASH_ASYNC_BUFFER_SIZE 16
#define FLASH_ASYNC_FLUSH_INTERVAL_MS 1000

/**
 * @brief Asynchronous flash logger handle
 */
typedef struct {
    // Double buffer for lock-free operation
    LogEntry_t buffer[2][FLASH_ASYNC_BUFFER_SIZE];
    volatile uint8_t write_buf;    // Currently writing to
    volatile uint8_t read_buf;     // Currently flushing
    volatile uint8_t write_idx;    // Next position in write buffer
    volatile uint8_t read_count;   // Entries pending in read buffer
    
    // Synchronization
    SemaphoreHandle_t flush_sem;
    SemaphoreHandle_t mutex;
    TaskHandle_t flush_task;
    
    // Statistics
    uint32_t entries_logged;
    uint32_t entries_dropped;
    uint32_t flush_errors;
    
    // Underlying synchronous logger
    FlashLogger_t* sync_logger;
    
    bool initialized;
} AsyncFlashLogger_t;

/**
 * @brief Initialize async flash logger
 * @param async Async logger handle
 * @param sync Underlying synchronous logger
 * @param ctx System context
 * @return true on success
 */
bool FlashLogger_AsyncInit(
    AsyncFlashLogger_t* async,
    FlashLogger_t* sync,
    SystemContext_t* ctx
);

/**
 * @brief Queue an entry for async logging
 * @param async Async logger
 * @param entry Entry to log
 * @return true if queued, false if dropped (buffer full)
 * @note Never blocks, returns immediately
 */
bool FlashLogger_AsyncWrite(
    AsyncFlashLogger_t* async,
    const LogEntry_t* entry
);

/**
 * @brief Force immediate flush
 * @param async Async logger
 * @param timeout_ms Maximum time to wait
 * @return true if flush completed
 */
bool FlashLogger_AsyncFlush(
    AsyncFlashLogger_t* async,
    uint32_t timeout_ms
);

/**
 * @brief Get async logger statistics
 */
void FlashLogger_AsyncGetStats(
    const AsyncFlashLogger_t* async,
    uint32_t* entries_logged,
    uint32_t* entries_dropped,
    uint32_t* flush_errors
);

#endif // FLASH_LOGGER_ASYNC_H
```

### 4.2 Implementation

```c
// flash_logger_async.c
#include "flash_logger_async.h"

static void Task_FlashFlush(void* pvParameters) {
    AsyncFlashLogger_t* async = pvParameters;
    
    while (1) {
        // Wait for signal or timeout
        if (xSemaphoreTake(async->flush_sem, 
            pdMS_TO_TICKS(FLASH_ASYNC_FLUSH_INTERVAL_MS)) == pdTRUE) {
            
            // Signal received - check if there's work
        }
        
        // Swap buffers atomically
        taskENTER_CRITICAL();
        uint8_t ready_buf = async->write_buf;
        async->read_count = async->write_idx;
        async->write_buf = 1 - async->write_buf;  // Swap
        async->write_idx = 0;
        taskEXIT_CRITICAL();
        
        // Flush ready buffer (outside critical section)
        for (int i = 0; i < async->read_count; i++) {
            if (!FlashLogger_Write(async->sync_logger, 
                                   &async->buffer[ready_buf][i])) {
                async->flush_errors++;
            }
        }
        
        async->entries_logged += async->read_count;
    }
}

bool FlashLogger_AsyncWrite(AsyncFlashLogger_t* async, const LogEntry_t* entry) {
    if (!async || !async->initialized || !entry) return false;
    
    // Try to acquire mutex with timeout
    if (xSemaphoreTake(async->mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        async->entries_dropped++;
        return false;  // Busy, drop entry
    }
    
    // Check if buffer full
    if (async->write_idx >= FLASH_ASYNC_BUFFER_SIZE) {
        xSemaphoreGive(async->mutex);
        async->entries_dropped++;
        return false;
    }
    
    // Copy entry
    memcpy(&async->buffer[async->write_buf][async->write_idx],
           entry, sizeof(LogEntry_t));
    async->write_idx++;
    
    xSemaphoreGive(async->mutex);
    
    // Signal flush task if buffer getting full
    if (async->write_idx >= FLASH_ASYNC_BUFFER_SIZE / 2) {
        xSemaphoreGive(async->flush_sem);
    }
    
    return true;
}
```

---

## 5. Modular Configuration

### 5.1 Configuration Hierarchy

```
config/
├── config_system.h      // Clock, version, debug
├── config_pwm.h         // PWM timing, limits
├── config_adc.h         // Sampling, filtering
├── config_pid.h         // Control parameters
├── config_safety.h      // Thresholds, limits
├── config_security.h    // Auth, crypto settings
├── config_freertos.h    // Task priorities, stacks
└── config.h             // Aggregates all
```

### 5.2 Example Configuration Files

```c
// config/config_safety.h
#ifndef CONFIG_SAFETY_H
#define CONFIG_SAFETY_H

#include <stdint.h>
#include <stdbool.h>

// Temperature thresholds
#define SAFETY_TEMP_WARNING_C       75.0f
#define SAFETY_TEMP_CRITICAL_C      85.0f
#define SAFETY_TEMP_SHUTDOWN_C      95.0f
#define SAFETY_TEMP_HYSTERESIS_C    2.0f

// Thermal runaway detection
#define SAFETY_THERMAL_RUNAWAY_THRESHOLD_C_PER_S  5.0f
#define SAFETY_THERMAL_RUNAWAY_SAMPLES            10
#define SAFETY_THERMAL_RUNAWAY_CONFIRMATION_MS    50

// Voltage limits
#define SAFETY_VOLTAGE_MIN_V        5.0f
#define SAFETY_VOLTAGE_MAX_V        30.0f
#define SAFETY_VOLTAGE_WARNING_LOW_V   6.0f
#define SAFETY_VOLTAGE_WARNING_HIGH_V  28.0f

// Current limits
#define SAFETY_CURRENT_MAX_A        10.0f
#define SAFETY_CURRENT_WARNING_A    8.0f
#define SAFETY_CURRENT_SENSE_OHMS   0.01f

// Watchdog configuration
#define SAFETY_WATCHDOG_TIMEOUT_MS     500
#define SAFETY_WATCHDOG_REFRESH_MS     100
#define SAFETY_WATCHDOG_WARNING_PCT    80

// Recovery settings
#define SAFETY_RECOVERY_MAX_ATTEMPTS   3
#define SAFETY_RECOVERY_BACKOFF_MS     5000
#define SAFETY_RECOVERY_BACKOFF_MULT   2.0f

// PWM degradation
#define SAFETY_PWM_DEGRADATION_DUTY    0.5f

// Compile-time validation
#if SAFETY_TEMP_SHUTDOWN_C < SAFETY_TEMP_CRITICAL_C
    #error "Shutdown temperature must be above critical temperature"
#endif

#if SAFETY_VOLTAGE_MAX_V <= SAFETY_VOLTAGE_MIN_V
    #error "Max voltage must be greater than min voltage"
#endif

#endif // CONFIG_SAFETY_H
```

```c
// config/config.h (aggregator)
#ifndef CONFIG_H
#define CONFIG_H

// Include all configuration modules
#include "config_system.h"
#include "config_pwm.h"
#include "config_adc.h"
#include "config_pid.h"
#include "config_safety.h"
#include "config_security.h"
#include "config_freertos.h"

// Version definition (uses config_system.h macros)
#define ADAPTIVEPWM_VERSION_STRING \
    ADAPTIVEPWM_STRINGIFY(ADAPTIVEPWM_VERSION_MAJOR) "." \
    ADAPTIVEPWM_STRINGIFY(ADAPTIVEPWM_VERSION_MINOR) "." \
    ADAPTIVEPWM_STRINGIFY(ADAPTIVEPWM_VERSION_PATCH)

#endif // CONFIG_H
```

---

## 6. Role-Based Access Control (RBAC)

### 6.1 RBAC Design

```c
// cli_rbac.h
#ifndef CLI_RBAC_H
#define CLI_RBAC_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief User roles for command authorization
 */
typedef enum {
    ROLE_NONE = 0,           // Unauthenticated
    ROLE_VIEWER,             // Read-only access
    ROLE_OPERATOR,           // Standard operations
    ROLE_MAINTENANCE,        // Diagnostics and calibration
    ROLE_ADMIN               // Full access including security
} UserRole_t;

#define ROLE_COUNT 5

/**
 * @brief Command access requirements
 */
typedef struct {
    const char* command;
    UserRole_t min_role;
    bool requires_auth;
    const char* description;
} CommandAccess_t;

// Role definitions
static const CommandAccess_t command_access[] = {
    // Public commands (no auth required)
    {"help",        ROLE_NONE,        false, "Show help"},
    {"version",     ROLE_NONE,        false, "Show version"},
    
    // Viewer commands (read-only, requires auth)
    {"status",      ROLE_VIEWER,      true,  "Show system status"},
    {"monitor",     ROLE_VIEWER,      true,  "Real-time monitoring"},
    
    // Operator commands
    {"pwm",         ROLE_OPERATOR,    true,  "PWM control"},
    {"start",       ROLE_OPERATOR,    true,  "Start operation"},
    {"stop",        ROLE_OPERATOR,    true,  "Stop operation"},
    
    // Maintenance commands
    {"calibrate",   ROLE_MAINTENANCE, true,  "Calibrate sensors"},
    {"faults",      ROLE_MAINTENANCE, true,  "View fault history"},
    {"diagnostic",  ROLE_MAINTENANCE, true,  "Diagnostic mode"},
    {"recovery",    ROLE_MAINTENANCE, true,  "Attempt recovery"},
    
    // Admin commands
    {"passwd",      ROLE_ADMIN,       true,  "Change password"},
    {"authstatus",  ROLE_ADMIN,       true,  "View auth status"},
    {"config",      ROLE_ADMIN,       true,  "System configuration"},
    {"reset",       ROLE_ADMIN,       true,  "System reset"},
};

#define COMMAND_ACCESS_COUNT (sizeof(command_access) / sizeof(command_access[0]))

/**
 * @brief Check if user can execute command
 * @param user_role Current user role
 * @param command Command to check
 * @return true if authorized
 */
bool CLI_RBAC_CanExecute(UserRole_t user_role, const char* command);

/**
 * @brief Get minimum role for command
 * @param command Command name
 * @return Minimum role required (ROLE_NONE if command unknown)
 */
UserRole_t CLI_RBAC_GetMinRole(const char* command);

/**
 * @brief Get role name as string
 */
const char* CLI_RBAC_GetRoleName(UserRole_t role);

/**
 * @brief Get commands available to role
 * @param role User role
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of bytes written
 */
uint16_t CLI_RBAC_GetAvailableCommands(UserRole_t role, char* buffer, uint16_t size);

#endif // CLI_RBAC_H
```

---

## 7. Hardware Abstraction Portability

### 7.1 HAL Interface Design

```c
// hal/hal_interface.h
#ifndef HAL_INTERFACE_H
#define HAL_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Platform abstraction layer
 * 
 * This interface allows porting AdaptivePWM to different platforms
 * by implementing these functions for the target hardware.
 */

// Platform-specific types
typedef void* PlatformPWM_t;
typedef void* PlatformADC_t;
typedef void* PlatformUART_t;
typedef void* PlatformTimer_t;

/**
 * @brief PWM platform interface
 */
typedef struct {
    bool (*init)(PlatformPWM_t* pwm, uint32_t frequency);
    bool (*start)(PlatformPWM_t pwm);
    bool (*stop)(PlatformPWM_t pwm);
    bool (*set_duty)(PlatformPWM_t pwm, float duty);
    float (*get_duty)(PlatformPWM_t pwm);
    void (*emergency_stop)(PlatformPWM_t pwm);
} PlatformPWM_Interface_t;

/**
 * @brief ADC platform interface
 */
typedef struct {
    bool (*init)(PlatformADC_t* adc, uint8_t num_channels);
    bool (*start)(PlatformADC_t adc);
    bool (*stop)(PlatformADC_t adc);
    bool (*read)(PlatformADC_t adc, uint8_t channel, float* value);
    bool (*read_all)(PlatformADC_t adc, float* values, uint8_t count);
} PlatformADC_Interface_t;

/**
 * @brief Platform registration
 * 
 * Called during system initialization to register platform drivers.
 */
typedef struct {
    const PlatformPWM_Interface_t* pwm;
    const PlatformADC_Interface_t* adc;
    // ... other interfaces
} PlatformInterface_t;

bool Platform_Register(const PlatformInterface_t* platform);
const PlatformInterface_t* Platform_Get(void);

#endif // HAL_INTERFACE_H
```

### 7.2 STM32 Implementation

```c
// hal/platform_stm32f4.c
#include "hal_interface.h"
#include "stm32f4xx_hal.h"

static bool stm32_pwm_init(PlatformPWM_t* pwm, uint32_t frequency) {
    TIM_HandleTypeDef* htim = pwm;
    // STM32-specific initialization...
    return HAL_TIM_PWM_Init(htim) == HAL_OK;
}

static bool stm32_pwm_set_duty(PlatformPWM_t pwm, float duty) {
    TIM_HandleTypeDef* htim = pwm;
    uint32_t pulse = (uint32_t)(duty * htim->Init.Period);
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, pulse);
    return true;
}

static const PlatformPWM_Interface_t stm32_pwm_interface = {
    .init = stm32_pwm_init,
    .start = stm32_pwm_start,
    .stop = stm32_pwm_stop,
    .set_duty = stm32_pwm_set_duty,
    .get_duty = stm32_pwm_get_duty,
    .emergency_stop = stm32_pwm_emergency_stop,
};

static const PlatformInterface_t stm32_platform = {
    .pwm = &stm32_pwm_interface,
    .adc = &stm32_adc_interface,
    // ...
};

void Platform_STM32F4_Init(void) {
    Platform_Register(&stm32_platform);
}
```

---

## 8. Migration Path

### 8.1 Phase 1: Add New Architecture (Backward Compatible)

1. Add `system_context.h` alongside existing code
2. Add `event_dispatcher.h` module
3. Create new HAL functions with `SystemContext_t*` parameter
4. Keep old functions as wrappers

```c
// Backward compatibility wrapper
bool Adaptive_PWM_Init_Legacy(Adaptive_PWM_t* pwm) {
    return Adaptive_PWM_Init(pwm, SystemContext_Get());
}
```

### 8.2 Phase 2: Migrate Module by Module

1. Start with leaf modules (HAL drivers)
2. Update tests to use context injection
3. Migrate control layer
4. Migrate safety layer
5. Finally migrate main.c

### 8.3 Phase 3: Remove Legacy

1. Mark old functions as deprecated
2. Update all call sites
3. Remove legacy wrappers
4. Remove global state

---

## 9. Testing Strategy

### 9.1 Mock HAL for Unit Testing

```c
// tests/mock_hal.h
#ifndef MOCK_HAL_H
#define MOCK_HAL_H

#include "hal_interface.h"

// Mock implementations that don't need hardware
typedef struct {
    float current_duty;
    bool is_running;
    bool emergency_stopped;
} MockPWMState_t;

void MockPWM_Init(void);
void MockPWM_GetState(MockPWMState_t* state);
void MockPWM_SetFault(bool fault);

// Register mock as platform
void Platform_Mock_Init(void);

#endif // MOCK_HAL_H
```

### 9.2 Unit Test Example

```c
// tests/test_pwm_with_context.c
#include "unity.h"
#include "mock_hal.h"
#include "system_context.h"

void setUp(void) {
    Platform_Mock_Init();
}

void test_PWM_SetDuty_RespectsSafetyLimits(void) {
    // Arrange
    SystemContext_t ctx;
    SystemContext_Init(&ctx);
    
    // Set safety limit to 50%
    EnhancedSafety_SetMaxDuty(ctx.safety, 0.5f);
    
    // Act - try to set 80%
    Adaptive_PWM_SetDuty(ctx.pwm, 0.8f);
    
    // Assert - should be clamped to 50%
    MockPWMState_t state;
    MockPWM_GetState(&state);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, state.current_duty);
}
```

---

## 10. Summary of Changes

### 10.1 Files Added

| File | Purpose |
|------|---------|
| `src/system_context.h/c` | Dependency injection container |
| `src/event_dispatcher.h/c` | Decoupled event system |
| `src/flash_logger_async.h/c` | Non-blocking flash logging |
| `config/config_*.h` | Modular configuration |
| `src/cli_rbac.h/c` | Role-based access control |
| `src/hal/hal_interface.h` | Platform abstraction |

### 10.2 Files Modified

| File | Changes |
|------|---------|
| `src/main.c` | Use context injection, remove globals |
| `src/hal_pwm.c` | Accept context pointer |
| `src/hal_adc.c` | Accept context pointer |
| `src/enhanced_safety.c` | Use event system |
| `src/cli_commands.c` | Add RBAC checks |
| `src/freertos_tasks.c` | Use context for handles |

### 10.3 Backward Compatibility

- Old API preserved as wrappers during migration
- Config.h aggregates new config files
- No breaking changes until Phase 3

---

**End of Design Update**
