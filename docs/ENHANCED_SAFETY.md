# Enhanced Safety & Fault Recovery System

**Task:** PWM-ARCH-004  
**Version:** 1.0.0  
**Date:** 2026-04-15

## Overview

The Enhanced Safety System provides comprehensive fault management with automatic recovery, graceful degradation, and predictive maintenance capabilities for the AdaptivePWM project.

## Features

### 1. Fault History Logging (fault_history.h/c)

- **Persistent Storage**: Faults logged to internal Flash memory (Sector 6)
- **Circular Buffer**: 256 entries with automatic wrapping
- **CRC Validation**: Each entry protected with CRC16 checksum
- **Survives Resets**: Fault history preserved across system restarts
- **Statistics Tracking**: Per-fault-type counters, recovery success/failure rates

#### Data Structure
```c
typedef struct {
    uint32_t timestamp_ms;        // System uptime when fault occurred
    uint32_t rtc_timestamp;       // Real-time clock if available
    FaultType_t fault_type;       // Type of fault
    FaultSeverity_t severity;     // Severity level
    uint16_t error_code;          // Original error code
    uint32_t context_data;        // Context-specific data
    RecoveryAction_t recovery_action; // Attempted recovery
    RecoveryResult_t recovery_result; // Result of recovery attempt
    uint8_t retry_count;          // Number of retry attempts
    uint16_t crc;                 // CRC16 checksum
} FaultEntry_t;
```

#### Supported Fault Types
- Over/Under Voltage
- Over Current
- Over Temperature
- Thermal Runaway
- PWM Fault
- ADC Failure
- Watchdog Timeout
- Communication Errors
- CRC Errors
- Config Corruption
- Hardware Faults
- Software Faults

### 2. Automatic Fault Recovery (enhanced_safety.h/c)

- **Configurable Recovery**: Per-fault-type recovery strategies
- **Retry Mechanism**: Configurable retry attempts with backoff
- **Recovery Actions**:
  - `RECOVERY_ACTION_RETRY`: Simple retry
  - `RECOVERY_ACTION_RESET`: Module reset
  - `RECOVERY_ACTION_DEGRADE`: Graceful degradation
  - `RECOVERY_ACTION_STOP`: Safe stop
  - `RECOVERY_ACTION_RESTART`: System restart
  - `RECOVERY_ACTION_MANUAL`: Require manual intervention

#### Recovery Configuration
```c
static const struct {
    FaultType_t fault;
    RecoveryAction_t action;
    uint8_t max_attempts;
    SafetyState_t failure_state;
} recovery_map[];
```

### 3. Graceful Degradation

When faults occur, the system can reduce functionality instead of stopping completely:

- **None**: Full operation
- **Light**: 90% performance (10% reduction)
- **Moderate**: 50% PWM duty limit
- **Severe**: 20% PWM duty limit
- **Critical**: Safe stop

```c
typedef enum {
    DEGRADATION_NONE = 0,
    DEGRADATION_LIGHT,
    DEGRADATION_MODERATE,
    DEGRADATION_SEVERE,
    DEGRADATION_CRITICAL
} DegradationLevel_t;
```

### 4. Multi-Level Watchdog Strategy

Independent watchdog monitoring for each subsystem:

| Module | Timeout | Warning | Auto-Recovery | Panic |
|--------|---------|---------|---------------|-------|
| Main   | 1000ms  | 800ms   | Yes           | Yes   |
| PWM    | 500ms   | 400ms   | Yes           | Yes   |
| ADC    | 250ms   | 200ms   | Yes           | No    |
| Comm   | 2000ms  | 1600ms  | No            | No    |
| Safety | 500ms   | 400ms   | Yes           | Yes   |

### 5. Predictive Maintenance

Analyzes fault patterns to predict maintenance needs:

```c
typedef struct {
    bool maintenance_recommended;
    uint32_t days_until_maintenance;
    float health_score;           // 0.0 - 100.0
    float degradation_rate;       // % per day
    uint32_t trend_direction;       // 0=stable, 1=improving, 2=degrading
    const char* primary_concern;
} MaintenancePrediction_t;
```

**Thresholds:**
- Health score < 70%: Maintenance recommended
- Fault rate > 10/day: Maintenance required
- Degradation rate > 2%/day: Immediate attention

### 6. CRC Validation

Critical safety data protected with CRC16:

```c
typedef struct __attribute__((packed)) {
    float max_duty_cycle;
    float max_current;
    float max_temp;
    uint32_t safety_flags;
    uint16_t config_version;
    uint16_t crc;
} CriticalSafetyData_t;
```

### 7. Diagnostic Mode

Extended logging for detailed analysis:

- Enabled via CLI: `diagnostic on`
- Auto-disables after 5 minutes for safety
- Increased flash wear - use sparingly
- Logs all state transitions and module health

## CLI Commands

### fault
Show fault history log
```
faults              # Show last 20 faults
faults clear        # Clear fault history
faults stats        # Show fault statistics
```

### diagnostic
Control diagnostic mode
```
diagnostic          # Show diagnostic status
diagnostic on       # Enable diagnostic mode
diagnostic off      # Disable diagnostic mode
```

### safety
Show safety system status
```
safety              # Show complete safety status
safety test         # Run self-test
```

### recovery
Recovery management
```
recovery            # Show recovery status
recovery request    # Request manual recovery attempt
```

### maintenance
Predictive maintenance
```
maintenance         # Show maintenance prediction
```

## Integration

### Initialization
```c
// In main.c
EnhancedSafetyManager_t safety_manager;

// Initialize
EnhancedSafety_Init(&safety_manager);
FaultHistory_Init();
```

### Fault Reporting
```c
// Report a fault
EnhancedSafety_ReportFault(&safety_manager, 
                             FAULT_TYPE_OVER_CURRENT,
                             FAULT_SEVERITY_ERROR,
                             ERR_OVER_CURRENT,
                             current_value);
```

### Watchdog Checkin
```c
// From each module
EnhancedSafety_WatchdogCheckin(&safety_manager, WDG_MODULE_PWM);
```

### Periodic Processing
```c
// Call periodically (e.g., every 100ms)
SafetyState_t state = EnhancedSafety_Process(&safety_manager);

if (state == SAFETY_STATE_EMERGENCY) {
    // Handle emergency stop
}
```

## Safety States

```
INIT → NORMAL → [DEGRADED_PWM/DEGRADED_ADC] → RECOVERY → NORMAL
              ↓
              → SAFE_STOP → RECOVERY → NORMAL
              ↓
              → EMERGENCY (requires manual reset)
              ↓
              → ERROR (unrecoverable)
```

## Flash Memory Layout

```
Sector 6 (0x080C0000 - 0x080DFFFF): Fault History
  ├── FlashHeader_t (32 bytes)
  ├── FaultEntry_t[256] (32 bytes each = 8KB)
  └── Total: ~8.5KB used

Sector 7 (0x080E0000 - 0x080FFFFF): General Logging
  └── Used by existing flash_logger
```

## Configuration

### Default Safety Configuration
```c
SafetyConfig_t default_config = {
    .auto_recovery_enabled = true,
    .graceful_degradation_enabled = true,
    .watchdog_enabled = true,
    .crc_validation_enabled = true,
    .recovery_backoff_ms = 5000,
    .max_recovery_attempts = 3,
    .pwm_degradation_duty = 0.5f,      // 50% in degraded mode
    .diagnostic_mode_timeout = 300000   // 5 minutes
};
```

### Critical Safety Data Defaults
```c
CriticalSafetyData_t default_critical_data = {
    .max_duty_cycle = 0.95f,      // 95% max
    .max_current = 10.0f,           // 10A max
    .max_temp = 85.0f,              // 85°C max
    .safety_flags = 0xFFFF,           // All features enabled
    .config_version = SAFETY_CONFIG_VERSION
};
```

## Testing

### Self-Test
```c
if (EnhancedSafety_SelfTest(&safety_manager)) {
    // Self-test passed
}
```

Tests performed:
1. Initialization check
2. CRC calculation validation
3. Critical data validation
4. Module health verification

### Manual Testing via CLI
```
> safety test
Running safety self-test...
Self-test PASSED

> faults stats
Fault Statistics:
  Total faults: 0
  Total recoveries: 0
  Failed recoveries: 0

> maintenance
Maintenance Prediction:
=======================
Health Score: 100.0%
Status: OK
Estimated maintenance in: 365 days
```

## Error Handling

### CRC Failures
- Logged as fault
- System continues with defaults
- Requires manual intervention to clear

### Flash Write Failures
- Reported via error handler
- Fault history may be lost
- System continues with runtime logging only

### Watchdog Timeouts
- Module marked unhealthy
- Auto-recovery attempted if enabled
- Escalation to emergency stop if critical

## Performance Impact

- **Flash writes**: ~2ms per fault entry
- **Flash erases**: ~200ms (rare, only on wrap)
- **Process() call**: ~100μs typical
- **Memory usage**: ~2KB RAM for manager state
- **Flash usage**: ~8.5KB for history storage

## Security Considerations

- **Audit Trail**: All faults logged with timestamp
- **Tamper Evidence**: CRC validation detects modifications
- **Access Control**: Clear fault history requires authentication
- **Safe Defaults**: CRC failures result in safe operating limits

## Future Enhancements

1. **RTC Integration**: Real-time timestamps instead of uptime
2. **Network Reporting**: Remote fault notification
3. **Machine Learning**: Anomaly detection in fault patterns
4. **Extended Storage**: External SPI flash for larger history
5. **Encrypted Storage**: Encrypted fault history for sensitive applications

## References

- **CISSP Domains**: 3 (Security Architecture), 7 (Security Operations), 8 (Software Development)
- **NIST CSF**: DE.AE (Anomalies and Events), PR.IP (Information Protection)
- **IEC 61508**: Functional Safety of Electrical/Electronic Systems
- **ISO 27001**: Information Security Management

## Author

AdaptivePWM Security Team  
Task: PWM-ARCH-004  
Date: 2026-04-15
