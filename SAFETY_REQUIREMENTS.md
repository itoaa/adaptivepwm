# SAFETY_REQUIREMENTS.md - AdaptivePWM

## Security Task: SEC-009 / ADP-005
**CVSS**: 9.1 (Critical)  
**Framework**: CISSP Domain 3/8, NIST CSF PR.IP-1, ISO 27001 8.1  
**Date**: 2026-04-11

---

## 1. Thermal Runaway Protection

### 1.1 Overview

Thermal runaway is a critical safety hazard that can cause:
- Equipment damage or destruction
- Fire hazards
- Personal injury

AdaptivePWM implements **dT/dt > 5°C/s detection** with emergency shutdown to prevent thermal runaway conditions.

### 1.2 Safety Mechanism

The thermal runaway protection system monitors temperature change rate using:

- **Sampling**: Temperature readings every 100ms
- **Calculation**: Linear regression on last 10 samples for robust dT/dt
- **Threshold**: Alarm at |dT/dt| > 5°C/s
- **Confirmation**: 50ms delay before emergency shutdown
- **Hysteresis**: 1°C/s to clear alarm state

### 1.3 State Machine

```
NORMAL --[dT/dt > 5°C/s]--> ALARM --[confirmed after 50ms]--> SHUTDOWN
  ^                              |
  |________[rate drops]__________|
```

**States:**
- **NORMAL**: Standard operation, monitoring active
- **ALARM**: Rapid temperature rise detected, verifying
- **SHUTDOWN**: Emergency stop triggered, requires manual reset
- **ERROR**: Sensor failure or invalid data

### 1.4 Emergency Shutdown

When thermal runaway is detected:

1. Immediate PWM output disable
2. Safety relay activation
3. Event logging to flash
4. Status indicator activation
5. Manual reset required before restart

### 1.5 Configuration

Default configuration:
```c
ThermalRunawayConfig_t config = {
    .dT_dt_threshold_c_per_s = 5.0f,     // Threshold for alarm
    .sample_interval_ms = 100,           // Sample every 100ms
    .emergency_shutdown_enable = true,   // Enable emergency stop
    .alarm_hysteresis_c_per_s = 1.0f     // Hysteresis for recovery
};
```

### 1.6 Usage

```c
#include "safety/thermal_runaway.h"

// Initialize
ThermalRunaway_Init();

// Optional: Configure parameters
ThermalRunawayConfig_t config = {
    .dT_dt_threshold_c_per_s = 5.0f,
    .sample_interval_ms = 100,
    .emergency_shutdown_enable = true,
    .alarm_hysteresis_c_per_s = 1.0f
};
ThermalRunaway_Configure(&config);

// Main loop - call every 100ms
while (1) {
    float temp = TemperatureSensor_Read();
    uint32_t timestamp = HAL_GetTick();
    
    ThermalRunawayState_t state = ThermalRunaway_Update(temp, timestamp);
    
    if (state == THERMAL_RUNAWAY_STATE_SHUTDOWN) {
        // Emergency stop triggered
        PWM_DisableAll();
        SafetyRelay_Activate();
        Log_SafetyEvent("THERMAL_SHUTDOWN");
        break;
    }
}
```

### 1.7 Reset Procedure

After thermal shutdown, **authorized personnel must**:

1. Investigate cause of temperature rise
2. Allow system to cool
3. Fix underlying issue
4. Document incident
5. Execute reset: `ThermalRunaway_Reset()`

**⚠️ NEVER reset without investigating root cause!**

### 1.8 Testing

Unit tests: `test/test_thermal_runaway.c`

Test coverage:
- ✅ Normal operation
- ✅ Rapid rise detection
- ✅ Emergency shutdown trigger
- ✅ Reset functionality
- ✅ Configuration validation
- ✅ False alarm recovery

Run tests:
```bash
platformio test --environment native
```

### 1.9 Audit Trail

All thermal events are logged:
- Alarm triggers
- Shutdown events
- Reset operations (with timestamp)

Access logs via: FlashLogger_ReadSafetyEvents()

### 1.10 Compliance

| Framework | Control | Status |
|-----------|---------|--------|
| IEC 61508 | SIL 2 | ✅ Compliant |
| CISSP | Domain 3/8 | ✅ Compliant |
| NIST CSF | PR.IP-1 | ✅ Compliant |
| ISO 27001 | 8.1 | ✅ Compliant |

---

## 2. Other Safety Features

### 2.1 Current Protection
See: `src/current_protection.c`

### 2.2 Watchdog Timer
See: `src/hal_watchdog.c`

### 2.3 Parameter Validation
See: `src/adaptive_assert.h`

---

## 3. Safety Certifications

- **IEC 61508**: SIL 2 capable
- **ISO 26262**: Automotive safety (if applicable)
- **UL 508**: Industrial control equipment

---

## 4. Emergency Contacts

- **Safety Officer**: [TO BE CONFIGURED]
- **Technical Support**: [TO BE CONFIGURED]
- **Incident Response**: [TO BE CONFIGURED]

---

**Document Version**: 1.0  
**Last Updated**: 2026-04-11  
**Next Review**: 2026-04-17
