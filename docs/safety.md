# Safety Protocols

## Overview

Safety is paramount in power electronics applications. This document outlines the safety measures implemented in AdaptivePWM v2.1.0, including clock system safety, and guidelines for safe operation.

**System:** STM32F401RE @ 84 MHz  
**Clock:** 16 MHz HSE → 84 MHz SYSCLK  
**Version:** 2.1.0

---

## Clock System Safety

### Clock Security System (CSS)

The CSS monitors the HSE oscillator and automatically switches to HSI on failure.

**Behavior:**
- Continuous HSE monitoring
- Automatic switch to HSI (16 MHz) on failure
- NMI interrupt generated
- System continues at reduced performance

**Implementation:**
```c
// CSS is enabled in HAL_RCC_OscConfig()
// No additional code required
```

### PLL Lock Detection

Hardware ensures PLL achieves lock before use.

**Checks:**
- PLLRDY flag in RCC_CR register
- Automatic timeout in HAL library
- Error handler called on failure

### Clock Failure Detection

| Failure | Detection | Response |
|-----------|-----------|----------|
| HSE Stop | CSS | Switch to HSI |
| PLL Unlock | PLLRDY | System reset |
| APB Overrun | Bus fault | Error handler |

---

## Hardware Safety Limits

### Clock-Related Limits

| Parameter | Minimum | Maximum | Notes |
|-----------|---------|---------|-------|
| SYSCLK | 0 | 84 MHz | STM32F401 max |
| HCLK | 0 | 84 MHz | Same as SYSCLK |
| PCLK1 | 0 | 42 MHz | APB1 max |
| PCLK2 | 0 | 84 MHz | APB2 max |
| ADC Clock | 0 | 42 MHz | PCLK2/2 |
| HSE | 4 MHz | 26 MHz | Crystal range |

### Electrical Parameters

- **Voltage Limits**: ±5% of nominal rating
- **Current Limits**: Continuous operation up to rated current
- **Temperature Limits**: Components derated above 85°C
- **Frequency Range**: 1kHz to 100kHz operational range (PWM)

### Duty Cycle Constraints

- **Minimum Duty Cycle**: 5% to prevent complete shutdown
- **Maximum Duty Cycle**: 95% to allow adequate dead time
- **Hard Limits**: 2% - 98% (safety cutoff)
- **Adjustment Rate**: Limited to prevent oscillation and overshoot

---

## Software Safety Mechanisms

### Parameter Validation

All measured values are checked against expected ranges:

| Parameter | Min | Max | Unit |
|-----------|-----|-----|------|
| Vin | 0 | 36 | V |
| Vout | 0 | 36 | V |
| Current | -15 | 15 | A |
| Temperature | -40 | 125 | °C |
| Duty Cycle | 0.02 | 0.98 | - |
| Frequency | 1000 | 100000 | Hz |

Values outside these ranges trigger safety protocols.

### Clock Validation

System verifies clock configuration at startup:

```c
// Clock check in main.c
SystemCoreClock = HAL_RCC_GetSysClockFreq();
if (SystemCoreClock != 84000000) {
    Error_Critical(NULL, ERR_INVALID_PARAMS, "Clock mismatch");
}
```

### Error Handling

Three levels of error handling:

1. **Soft Errors**: Automatic recovery attempts
   - ADC timeout → Retry
   - UART error → Reinitialize

2. **Hard Errors**: System enters safe mode
   - PWM stopped
   - Default duty cycle set
   - Error logged

3. **Critical Errors**: Complete system shutdown
   - PWM emergency stop
   - Infinite loop with watchdog refresh
   - System reset on timeout

### Watchdog Functions

- **Type:** Independent Watchdog (IWDG)
- **Clock:** 32 kHz LSI (independent)
- **Timeout:** 500 ms
- **Refresh Interval:** 100 ms

**Implementation:**
```c
// Watchdog refreshed in:
// - SysTick_Handler (every 1 ms)
// - Main loop
// - Task functions

void SysTick_Handler(void) {
    HAL_IncTick();
    Adaptive_WDG_Refresh();  // Every ms
}
```

**Failure Modes:**
- Software deadlock → Watchdog reset
- Clock failure → Watchdog reset (LSI independent)
- Infinite loop → Watchdog reset

---

## Fail-Safe Procedures

### Clock Failure

**Scenario:** HSE stops or becomes unstable

**Response:**
1. CSS detects failure
2. Automatic switch to HSI (16 MHz)
3. NMI interrupt generated
4. System continues at 16 MHz
5. Error logged

**Recovery:**
- Manual HSE restart required
- Reconfigure PLL if needed
- Verify clock before resuming operation

### ADC Failure

**Scenario:** ADC conversion timeout or error

**Response:**
1. Stop PWM (safe state)
2. Log error
3. Attempt reinitialization
4. Resume if successful

### PWM Failure

**Scenario:** Timer fault or break input triggered

**Response:**
1. Immediate PWM shutdown via break
2. Set outputs to inactive state
3. Log error
4. Require manual restart

### Communication Loss

**Scenario:** UART timeout or protocol error

**Response:**
- Maintain last known good settings
- Continue autonomous operation
- Log error

---

## Clock Configuration Safety

### Startup Sequence

```
1. Power on
2. HSE startup (2-5 ms typical)
3. PLL lock (200 µs typical)
4. Clock switch to PLL
5. Flash latency set (2 WS for 84 MHz)
6. Peripheral clocks enabled
7. Safety checks
```

### Critical Settings

| Setting | Value | Safety Impact |
|---------|-------|---------------|
| Flash Latency | 2 WS | Required for 84 MHz |
| Voltage Scale | 2 | Required for 84 MHz |
| HSE Bypass | OFF | Crystal oscillator |
| CSS | ON | Failure detection |

### Forbidden Configurations

- **Overclocking:** SYSCLK > 84 MHz (undefined behavior)
- **APB1 > 42 MHz:** Bus errors guaranteed
- **No flash latency:** Corruption at high speeds
- **PLL without HSE:** Unstable for precision timing

---

## Testing Protocols

### Unit Tests

Each safety function has associated unit tests verifying:
- Normal operation
- Boundary conditions
- Error conditions

### Clock Tests

| Test | Method | Expected Result |
|------|--------|-----------------|
| HSE startup | Scope on OSC_IN | 16 MHz sine wave |
| PLL lock | Read PLLRDY | Set after ~200 µs |
| CSS trigger | Short HSE to GND | Switch to HSI |
| Watchdog | Skip refresh | Reset after 500 ms |

### Hardware Tests

Complete system testing under:
- Nominal conditions
- Stress conditions
- Fault conditions

**Clock Stress Test:**
1. Verify 84 MHz SYSCLK
2. Verify 42 MHz PCLK1
3. Verify 84 MHz PCLK2
4. Measure jitter on MCO pin
5. Test CSS response

---

## Compliance Standards

Design follows relevant standards:
- IEC 60950 (Information technology equipment)
- IEC 62477 (Power supplies)
- UL 60950 (North American requirements)
- IEC 61508 (Functional safety) - partial

---

## Risk Assessment

| Hazard | Likelihood | Severity | Mitigation |
|--------|------------|----------|------------|
| Clock failure | Low | High | CSS, Watchdog |
| Overcurrent | Low | High | Current limiting |
| Overvoltage | Low | High | Voltage clamping |
| Component failure | Medium | Medium | Redundancy |
| Software error | Low | High | Error checking, Watchdog |
| Thermal runaway | Low | High | Temperature monitoring |

---

## Maintenance Guidelines

Regular checks recommended:

### Daily (Runtime)
- Watchdog refreshes
- Clock status
- Temperature readings

### Monthly
- Parameter calibration
- Safety function verification
- Clock accuracy check

### Quarterly
- Full safety system test
- Clock jitter measurement
- HSE crystal check

### Annually
- Full system inspection
- Component replacement if needed
- Calibration certificate update

---

## Emergency Procedures

### Clock System Failure

**Symptoms:**
- System runs slow or erratic
- CSS interrupt triggered
- HSE_RDY cleared

**Actions:**
1. Check crystal connections
2. Verify HSE capacitors
3. Replace crystal if damaged
4. Restart system

### Complete System Failure

**Actions:**
1. Remove power
2. Check all connections
3. Verify 3.3V supply
4. Check crystal oscillation
5. Power cycle
6. Check error logs

---

## References

- STM32F401 Reference Manual (RM0368) - Clock section
- STM32F4 Flash Programming Manual
- AN2867: Oscillator design guide
- AN3307: Clock configuration for STM32F4
