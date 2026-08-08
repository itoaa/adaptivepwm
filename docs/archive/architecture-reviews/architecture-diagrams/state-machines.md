# State Machine Documentation

## Overview

This document describes the state machines in the AdaptivePWM system.

## Safety State Machine

The Enhanced Safety System manages the overall safety state of the system.

```mermaid
stateDiagram-v2
    [*] --> INIT: System Startup
    
    INIT --> NORMAL: EnhancedSafety_Init() success
    INIT --> ERROR: Initialization failure
    
    NORMAL --> DEGRADED_PWM: PWM fault
    NORMAL --> DEGRADED_ADC: ADC fault
    NORMAL --> RECOVERY: Recoverable fault
    NORMAL --> SAFE_STOP: Stop request
    NORMAL --> EMERGENCY: Critical fault
    
    DEGRADED_PWM --> NORMAL: Recovery success
    DEGRADED_PWM --> RECOVERY: Recovery attempt
    DEGRADED_PWM --> EMERGENCY: Critical fault
    
    DEGRADED_ADC --> NORMAL: Recovery success
    DEGRADED_ADC --> RECOVERY: Recovery attempt
    DEGRADED_ADC --> EMERGENCY: Critical fault
    
    RECOVERY --> NORMAL: Recovery success
    RECOVERY --> DEGRADED_PWM: Recovery failed (PWM)
    RECOVERY --> DEGRADED_ADC: Recovery failed (ADC)
    RECOVERY --> ERROR: Recovery failed (other)
    
    SAFE_STOP --> NORMAL: Manual resume
    SAFE_STOP --> EMERGENCY: Critical fault
    
    EMERGENCY --> SAFE_STOP: Clear emergency
    
    ERROR --> [*]: System shutdown
```

### State Descriptions

| State | Description | Entry Actions | Exit Conditions |
|-------|-------------|---------------|-----------------|
| `INIT` | System initialization | - Initialize modules<br/>- Load critical data | Init complete or failed |
| `NORMAL` | Full operation | - Clear degradations<br/>- Normal PWM limits | Fault detected or stop requested |
| `DEGRADED_PWM` | PWM in degraded mode | - Reduce duty cycle limit<br/>- Log degradation | Recovery success |
| `DEGRADED_ADC` | ADC in degraded mode | - Use estimated values<br/>- Reduce control frequency | Recovery success |
| `RECOVERY` | Attempting recovery | - Log recovery attempt<br/>- Execute recovery action | Success/failure |
| `SAFE_STOP` | Controlled shutdown | - Stop PWM<br/>- Safe GPIO state | Manual resume |
| `EMERGENCY` | Emergency stop | - Immediate PWM stop<br/>- Log emergency | Manual clear |
| `ERROR` | Fatal error state | - Log error<br/>- Safe shutdown | - |

## Recovery State Machine

```mermaid
stateDiagram-v2
    [*] --> IDLE: No recovery needed
    
    IDLE --> ATTEMPT_1: Recovery triggered
    
    ATTEMPT_1 --> BACKOFF: Attempt failed
    ATTEMPT_1 --> SUCCESS: Attempt success
    
    BACKOFF --> ATTEMPT_2: Backoff complete
    
    ATTEMPT_2 --> BACKOFF: Attempt failed
    ATTEMPT_2 --> SUCCESS: Attempt success
    
    BACKOFF --> ATTEMPT_3: Backoff complete
    
    ATTEMPT_3 --> BACKOFF: Attempt failed
    ATTEMPT_3 --> SUCCESS: Attempt success
    
    BACKOFF --> FAILED: Max attempts reached
    
    SUCCESS --> IDLE: Return to normal
    
    FAILED --> IDLE: Enter degraded state
    
    note right of BACKOFF
        Wait recovery_backoff_ms
        before next attempt
    end note
```

### Recovery Actions

| Recovery Action | Description | Used For |
|-----------------|-------------|----------|
| `RETRY` | Simple delay and retry | Communication errors |
| `RESET` | Reset affected module | PWM/ADC faults |
| `DEGRADE` | Enter degraded mode | Non-critical faults |
| `STOP` | Safe stop | Serious faults |
| `RESTART` | System restart | Software faults |
| `MANUAL` | Requires operator | Hardware/config faults |
| `NONE` | No action | INFO level faults |

## Degradation Level State

```mermaid
stateDiagram-v2
    [*] --> NONE: Normal operation
    
    NONE --> LIGHT: Warning threshold
    
    LIGHT --> NONE: Recovery success
    LIGHT --> MODERATE: Fault persists
    
    MODERATE --> LIGHT: Partial recovery
    MODERATE --> SEVERE: Multiple faults
    
    SEVERE --> MODERATE: Partial recovery
    SEVERE --> CRITICAL: Critical fault
    
    CRITICAL --> [*]: System shutdown
```

### Degradation Effects

| Level | Duty Cycle Limit | Current Limit | Effect |
|-------|------------------|---------------|--------|
| `NONE` | 95% | 10A | Full operation |
| `LIGHT` | 85% (90% of max) | 9A | Slight reduction |
| `MODERATE` | 50% (configurable) | 7A | Significant reduction |
| `SEVERE` | 20% | 5A | Minimal operation |
| `CRITICAL` | 0% | 0A | No operation |

## Watchdog Module State

```mermaid
stateDiagram-v2
    [*] --> HEALTHY: Module initialized
    
    HEALTHY --> DEGRADED: Timeout detected
    HEALTHY --> UNHEALTHY: Repeated timeouts
    
    DEGRADED --> HEALTHY: Checkin received
    DEGRADED --> UNHEALTHY: No recovery
    
    UNHEALTHY --> HEALTHY: Manual reset
    UNHEALTHY --> PANIC: Panic_on_timeout enabled
    
    PANIC --> EMERGENCY: Trigger emergency stop
```

## Task State Machine

```mermaid
stateDiagram-v2
    [*] --> STOPPED: Task not created
    
    STOPPED --> RUNNING: vTaskCreate
    
    RUNNING --> SUSPENDED: vTaskSuspend
    RUNNING --> BLOCKED: Waiting on resource
    RUNNING --> DELETED: vTaskDelete
    
    SUSPENDED --> RUNNING: vTaskResume
    
    BLOCKED --> RUNNING: Resource available
    
    DELETED --> [*]: Task cleanup
```

## System Initialization State

```mermaid
stateDiagram-v2
    [*] --> CLOCK_CONFIG: HAL_Init
    
    CLOCK_CONFIG --> WATCHDOG_INIT: SystemClock_Config
    
    WATCHDOG_INIT --> ERROR_INIT: Watchdog OK
    
    ERROR_INIT --> SAFETY_INIT: Error_Init
    
    SAFETY_INIT --> FAULT_INIT: EnhancedSafety_Init
    
    FAULT_INIT --> TEMP_INIT: FaultHistory_Init
    
    TEMP_INIT --> PARAM_INIT: TempMonitor_Init
    
    PARAM_INIT --> PWM_INIT: ParamCalc_Init
    
    PWM_INIT --> ADC_INIT: Adaptive_PWM_Init
    
    ADC_INIT --> UART_INIT: Adaptive_ADC_Init
    
    UART_INIT --> CLI_INIT: Adaptive_UART_Init
    
    CLI_INIT --> TASKS_INIT: CLI_Init
    
    TASKS_INIT --> SCHEDULER: Tasks_Init
    
    SCHEDULER --> [*]: vTaskStartScheduler
    
    note right of SCHEDULER
        FreeRTOS scheduler
        takes control
    end note
```

## UART CLI State Machine

```mermaid
stateDiagram-v2
    [*] --> DISCONNECTED: UART initialized
    
    DISCONNECTED --> AUTH_REQUIRED: Connection established
    
    AUTH_REQUIRED --> AUTHENTICATED: Login successful
    AUTH_REQUIRED --> AUTH_REQUIRED: Login failed
    
    AUTHENTICATED --> COMMAND: Command received
    
    COMMAND --> AUTHENTICATED: Command complete
    COMMAND --> AUTH_REQUIRED: Session timeout
    
    AUTHENTICATED --> DISCONNECTED: Disconnect
    AUTH_REQUIRED --> DISCONNECTED: Disconnect
```

## Diagnostic Mode State

```mermaid
stateDiagram-v2
    [*] --> NORMAL_MODE: Normal operation
    
    NORMAL_MODE --> DIAGNOSTIC: EnterDiagnosticMode()
    
    DIAGNOSTIC --> DIAGNOSTIC: Process commands
    DIAGNOSTIC --> NORMAL_MODE: ExitDiagnosticMode()
    DIAGNOSTIC --> NORMAL_MODE: Timeout (default: 30s)
    
    note right of DIAGNOSTIC
        Extended logging enabled
        Recovery attempts logged
        Detailed fault info
    end note
```
