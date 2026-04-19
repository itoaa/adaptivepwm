# AdaptivePWM Documentation

## Project Overview

AdaptivePWM is a real-time control system for buck/boost converters and electronic speed controllers (ESCs). It continuously monitors electrical parameters and dynamically adjusts PWM output for optimal efficiency.

**Version:** 2.3.1  
**Target:** STM32F401RE @ 84 MHz  
**Clock:** 16 MHz HSE → 84 MHz SYSCLK  
**Framework:** CISSP-Aligned Security Framework

---

## What's New in v2.3.1

### Security Framework (CISSP/NIST Aligned)
- **SEC-031:** Physical button confirmation for first-time password setup
- **SEC-049:** Secure bootloader with Ed25519 signature verification
- **SEC-043:** Formal risk assessment with CVSS 3.1 scoring
- **CLI Authentication:** Password-based login with session management
- **Flash Logger HMAC:** Integrity protection for log entries (pending)
- **Secure Boot:** Anti-rollback, RDP Level 2, recovery mode

### Enhanced Control Systems
- **Full PID Control:** Now with integral (Ki=0.01) and derivative (Kd=0.001) terms
- **Setpoint Weighting:** Reduces overshoot while maintaining response speed (default: 0.7)
- **Derivative on Measurement:** Prevents derivative kick on setpoint changes
- **PWM Ramping:** Smooth duty cycle transitions with configurable rate limiting
- **Feedforward Control:** Buck/boost converter topology compensation

### Improved Signal Processing
- **Dual-Stage Filtering:** Moving average (8 samples) + IIR for superior noise rejection
- **Transient Detection:** Automatically detects load changes
- **Adaptive Sampling:** Framework for increased sampling during transients
- **Channel-Optimized ADC:** 3/15/28 cycle sampling times per channel

### Performance Improvements
- ~6dB noise reduction from dual filtering
- Reduced overshoot through setpoint weighting
- Smoother transitions with ramping
- Better transient response

---

## Clock System (v2.1.0 Optimized)

### Configuration
```
HSE: 16 MHz external crystal
PLL: M=16, N=336, P=4, Q=7
SYSCLK: 84 MHz (maximum)
HCLK: 84 MHz
PCLK1: 42 MHz (APB1 - ADC, UART)
PCLK2: 84 MHz (APB2 - TIM1 PWM)
```

### Optimizations
- **ADC Clock:** 42 MHz (maximum, PCLK2/2)
- **PWM Clock:** 84 MHz (full resolution)
- **Sampling:** Optimized per channel (3/15/28 cycles)
- **Latency:** <50 µs (ADC → PWM)

See [docs/design.md](docs/design.md) for complete clock documentation.

---

## Features

### Implemented Components

1. **PWM Hardware Abstraction** (`hal_pwm.h/c`)
   - TIM1 complementary PWM with dead-time
   - Frequency: 20kHz @ 84 MHz clock
   - Duty cycle: 5% - 95% with hardware limits
   - Emergency stop via break input
   - **Setpoint ramping** (configurable rate)
   - **Feedforward control** for converters

2. **ADC Hardware Abstraction** (`hal_adc.h/c`)
   - 4-channel DMA-based sampling
   - Clock: 42 MHz (maximum)
   - Channels: Vin, Vout, Current, Temperature
   - Optimized sampling times per channel
   - Sample rate: 10kHz total
   - **Dual-stage filtering** (moving avg + IIR)
   - **Transient detection**

3. **Parameter Calculation** (`param_calc.h/c`)
   - Real-time L, C, ESR calculation
   - RMS-based ripple detection
   - Formulas:
     - L = (Vin - Vout) × D / (fsw × ΔI)
     - C = ΔI × D / (fsw × ΔV)
     - ESR = ΔV / ΔI (simplified)

4. **PID Controller** (`pid_controller.c`)
   - Full PID with anti-windup
   - **Setpoint weighting** (0-1)
   - **Derivative on measurement**
   - **Derivative filtering** (low-pass)
   - **Back-calculation anti-windup**
   - Runtime gain adjustment

5. **FreeRTOS Tasks** (`freertos_tasks.h/c`)
   - Measurement task: 1kHz
   - Control task: 100Hz
   - Safety task: 100Hz (highest priority)
   - CLI task: 50Hz

6. **Safety Systems** (`enhanced_safety.h/c`)
   - Independent watchdog (IWDG)
   - Temperature monitoring with derating
   - Overcurrent detection
   - Emergency PWM shutdown
   - **Fault recovery state machine**
   - **Multi-level safety states**

7. **CLI Interface** (`cli_commands.h/c`, `cli_auth.h/c`)
   - Commands: status, config, monitor, pwm, calibrate, errors, help
   - **Password authentication with session management**
   - **First-time setup with physical confirmation**
   - UART: 115200 baud
   - Interrupt-driven RX

8. **Error Handler** (`error_handler.h/c`)
   - 4 severity levels: INFO, WARNING, ERROR, CRITICAL
   - Circular log buffer (16 entries)
   - Automatic shutdown on critical errors

9. **Flash Logger** (`flash_logger.h/c`, `flash_logger_hmac.h/c`)
   - Persistent data logging
   - Circular buffer in flash sector 11
   - CRC validation
   - **HMAC-SHA256 integrity protection** (pending)

10. **Temperature Monitor** (`temperature_monitor.h/c`)
    - Thermal derating curve
    - Warning: 75°C
    - Critical: 85°C
    - Shutdown: 95°C

11. **Calibration** (`calibration.h/c`)
    - Automatic gain/offset calibration
    - Flash storage

12. **Secure Bootloader** (`bootloader/`)
    - Ed25519 signature verification
    - SHA-256 hash validation
    - Anti-rollback protection
    - RDP Level 2 support
    - Recovery mode

---

## Security Framework

### Implemented Security Controls

| Control | Implementation | Framework Alignment |
|---------|---------------|---------------------|
| **Authentication** | PBKDF2-HMAC-SHA256 (100,000 iterations) | NIST SP 800-132 |
| **Physical Confirmation** | GPIO button/jumper (SEC-031) | IEC 62443 |
| **Boot Integrity** | Ed25519 + SHA-256 (SEC-049) | NIST CSF PR.IP-1 |
| **Flash Protection** | HMAC-SHA256 (pending) | CISSP Domain 8 |
| **Session Management** | Timeout, privilege levels | NIST CSF PR.AC-1 |
| **Risk Assessment** | CVSS 3.1 scoring (SEC-043) | NIST RMF |

### Security Documentation
- [Threat Model](docs/THREAT-MODEL.md) - STRIDE analysis
- [Risk Assessment](docs/risk-assessment.md) - 30 risks identified
- [Risk Register](docs/risk-register.md) - Mitigation tracking
- [MISRA Compliance](docs/MISRA_COMPLIANCE.md) - Coding standards
- [Secure Boot](docs/secure-boot.md) - Bootloader documentation

---

## File Structure

```
src/
├── main.c                 # Entry point (clock config)
├── hal_pwm.h/c           # PWM driver with ramping & feedforward
├── hal_adc.h/c           # ADC driver with dual filtering
├── hal_uart.h/c          # UART driver
├── hal_watchdog.h/c      # Watchdog driver
├── param_calc.h/c        # L/C/ESR calculations
├── pid_controller.c      # PID with enhancements
├── freertos_tasks.h/c    # RTOS tasks
├── error_handler.h/c     # Error management
├── enhanced_safety.h/c   # Safety framework
├── cli_commands.h/c      # CLI implementation
├── cli_auth.h/c          # Authentication (SEC-031)
├── setup_gpio.h/c        # Physical confirmation GPIO
├── flash_logger.h/c      # Data logging
├── flash_logger_hmac.h/c # HMAC integrity
├── temperature_monitor.h/c # Thermal management
├── calibration.h/c       # Calibration routines
├── current_protection.h  # Overcurrent protection
└── config.h              # Central configuration

bootloader/
├── secure_bootloader.c/h # Ed25519 verification
├── ldscripts/            # Linker scripts
└── Makefile              # Bootloader build

docs/
├── architecture/         # Architecture docs (PWM-ARCH-007)
├── security/             # Security documentation
├── THREAT-MODEL.md       # STRIDE analysis
├── risk-assessment.md    # Risk assessment
├── risk-register.md      # Risk register
├── secure-boot.md        # Secure boot docs
├── MISRA_COMPLIANCE.md   # Coding standards
└── (other docs)

include/
├── mbedtls/              # mbedtls config
└── (headers)

tests/
├── security/             # Security tests
├── test_*.c              # Unit tests
└── (test binaries)

tools/
└── sign_firmware.py      # Ed25519 signing

keys/
├── bootloader_private.pem
├── bootloader_public.pem
└── bootloader_public_key.c
```

---

## Building

### Requirements
- PlatformIO
- STM32 CubeMX HAL

### Build Commands
```bash
# Release build (optimized)
pio run -e nucleo_f401re

# Debug build with CLI
pio run -e nucleo_f401re_debug

# Profile build
pio run -e nucleo_f401re_profile

# Upload
pio run --target upload

# Monitor serial output
pio device monitor -b 115200
```

### Build Status
- **RAM:** 7.5% (7,344 bytes from 98,304)
- **Flash:** 9.1% (47,908 bytes from 524,288)
- **Version:** 2.3.1

---

## Usage

### Hardware Connections

| Signal | Pin | Function | Clock |
|--------|-----|----------|-------|
| PWM_CH1 | PA8 | Main PWM output | 84 MHz |
| PWM_CH1N | PA9 | Complementary PWM | 84 MHz |
| ADC_VIN | PA0 | Input voltage | 42 MHz |
| ADC_VOUT | PA1 | Output voltage | 42 MHz |
| ADC_I | PA2 | Current sense | 42 MHz |
| ADC_T | PA3 | Temperature | 42 MHz |
| UART_TX | PA2 | Serial TX | 42 MHz |
| UART_RX | PA3 | Serial RX | 42 MHz |
| **BTN_USER** | **PC13** | **Setup confirmation** | **-** |
| **JP_ALT** | **PA0** | **Alternative confirmation** | **-** |

### Crystal Requirements
- **Frequency:** 16 MHz
- **Type:** HSE (High Speed External)
- **Accuracy:** ±20 ppm recommended
- **Load capacitance:** As per crystal datasheet (typically 8-20 pF)

### CLI Commands

```
status [adc|pwm|params]  - Show system status
config <param> <value>   - Configure system
monitor [duration]         - Real-time monitoring
pwm <duty|start|stop>      - PWM control
calibrate <vin> <vout>   - Calibrate ADC
errors [clear]            - Show/clear error log
help [command]           - Show help
login                      - Authenticate (requires password)
logout                     - End session
setup                      - First-time password setup (requires button)
```

### First-Time Setup (SEC-031)
1. Device boots in setup mode (no password configured)
2. User runs `setup` command via CLI
3. User physically presses PC13 button for 2+ seconds
4. Password is configured and stored in flash
5. Subsequent boots require authentication

---

## Safety Features

### Hardware Protection
- Watchdog timer: 500ms timeout
- PWM dead-time: 400ns
- Break input (fault detection)
- Duty cycle limits: 5% - 95%

### Software Protection
- Temperature derating
- Overcurrent detection
- Parameter validation
- Emergency shutdown
- **Fault recovery with state machine**
- **Multi-level safety states**

### Security Protection
- Password-based authentication
- Session timeout (5 minutes)
- Physical confirmation for setup
- Secure boot with signature verification
- Flash integrity protection

### Clock Safety
- CSS (Clock Security System) enabled
- HSE failure detection
- Automatic fallback to HSI

---

## Algorithms

### PID Control (Enhanced)
```
Proportional: P = Kp * (b*setpoint - measurement)
Integral:     I += Ki * error * dt (with anti-windup)
Derivative:   D = Kd * filtered_d(measurement)/dt
Output:       Out = P + I + D (clamped)

Where:
- b = setpoint_weight (0-1, default 0.7)
- Anti-windup via integral clamping
- Derivative filtered with alpha (default 0.1)
- Back-calculation when output saturated
```

### Efficiency Calculation
```
Efficiency = 1 - (Switching Losses + Conduction Losses)
Switching Losses = fsw × L × D²
Conduction Losses = ESR × I²
```

### Duty Cycle Control
```
With Feedforward (buck):
D_estimated = Vout / Vin
D_control = D_estimated + PID_output

With Ramping:
D_actual += min(max_change, D_target - D_current)
```

### Dual Filtering (ADC)
```
Stage 1 (Moving Average):
  avg = sum(last_8_samples) / 8

Stage 2 (IIR):
  filtered += alpha * (avg - filtered)
```

---

## Configuration

### PID Settings (`config.h`)
```c
#define DUTY_KP                     0.05f
#define DUTY_KI                     0.01f
#define DUTY_KD                     0.001f
#define PID_SETPOINT_WEIGHT         0.7f
#define PID_DERIVATIVE_FILTER       0.1f
```

### PWM Ramping (`config.h`)
```c
#define PWM_RAMP_ENABLED            1
#define PWM_RAMP_RATE_PER_SEC       0.10f       // 10% per second
```

### ADC Filtering (`config.h`)
```c
#define ADC_FILTER_IIR_ENABLED      1
#define ADC_FILTER_IIR_ALPHA        0.1f
#define ADC_FILTER_MOVING_AVG_ENABLED 1
#define ADC_FILTER_MOVING_AVG_SIZE  8
#define ADC_ADAPTIVE_SAMPLING_ENABLED 1
#define ADC_TRANSIENT_THRESHOLD     0.05f       // 5%
```

### Security Settings (`config.h`)
```c
#define CLI_AUTH_ENABLED            1
#define CLI_AUTH_HASH_ITERATIONS    100000      // PBKDF2 iterations
#define CLI_AUTH_SESSION_TIMEOUT_S  300         // 5 minutes
#define SETUP_CONFIRM_ENABLED       1           // Physical confirmation
#define SETUP_CONFIRM_GPIO_PORT     GPIOC
#define SETUP_CONFIRM_GPIO_PIN      GPIO_PIN_13 // PC13 button
```

---

## Testing

### Unit Tests
```bash
# Run all tests
cd tests
make test

# Individual tests
./test_thread_safety
./test_efficiency_calc
./benchmark_pbkdf2
```

### Security Tests
```bash
cd tests/security
make
./test_secure_boot
./test_auth_timing
./test_pbkdf2_iterations
```

### Hardware Tests
1. Clock verification (scope on MCO pin)
2. PWM output verification (scope)
3. ADC accuracy test (known voltage sources)
4. Parameter calculation validation
5. Safety system response test
6. PID step response test
7. Ramping behavior verification
8. Filter noise rejection test
9. **Authentication flow test**
10. **Secure boot verification**

---

## Changelog

### v2.3.1 (2026-04-19) - Security Framework
- **Security:** CISSP/NIST aligned security framework
- **SEC-031:** Physical button confirmation for setup
- **SEC-049:** Secure bootloader with Ed25519
- **SEC-043:** Formal risk assessment (30 risks)
- **CLI:** Password authentication with sessions
- **Documentation:** Threat model, risk register, MISRA compliance
- **Build:** Fixed STM32F401 compilation (no hardware RNG)

### v2.2.1 (2026-04-10) - Control System Enhancements
- **PID:** Added setpoint weighting, derivative on measurement, derivative filtering
- **PWM:** Added ramping, feedforward control, immediate mode
- **ADC:** Added dual filtering, transient detection, adaptive sampling framework
- **Config:** Enhanced with new control parameters
- **Documentation:** Updated with new features

### v2.1.1 (2026-03-22) - Security Framework
- Security & Safety Framework v1.1.0
- Threat modeling documentation
- Enhanced safety requirements

### v2.1.0 (2026-03-21) - Clock Optimization
- Clock system optimized for 16 MHz HSE
- ADC at maximum 42 MHz clock
- PWM at full 84 MHz resolution
- Complete clock system documentation

### v2.0.0 (2026-02-27)
- Initial release with FreeRTOS
- Basic PWM and ADC drivers
- CLI interface

---

## Future Enhancements

- [ ] CAN bus communication
- [ ] Machine learning optimization
- [ ] Advanced predictive algorithms
- [ ] Multi-channel support
- [ ] Ethernet connectivity
- [ ] ADC-PWM hardware synchronization
- [ ] Auto-tuning for PID gains
- [ ] Model predictive control (MPC)
- [ ] Hardware RNG on STM32F405/F407
- [ ] mbedTLS integration for HMAC

---

## Documentation

- [Changelog](CHANGELOG.md) - Version history
- [Improvements](IMPROVEMENTS.md) - v2.2.1 enhancements detail
- [Project Framework](PROJECT_FRAMEWORK.md) - Security framework
- [Setup Guide](SETUP_GUIDE.md) - Developer setup
- [Clock Design](docs/design.md) - Clock system documentation
- [Safety](docs/safety.md) - Safety protocols
- [API Reference](docs/api.md) - API documentation
- [Threat Model](docs/THREAT-MODEL.md) - Security threats
- [Risk Assessment](docs/risk-assessment.md) - Risk analysis
- [Secure Boot](docs/secure-boot.md) - Bootloader docs
- [MISRA Compliance](docs/MISRA_COMPLIANCE.md) - Coding standards

---

## References

- Arduino PID Library (br3ttb) - PID algorithm inspiration
- PIDPWM (AdysTech) - RTOS PID implementation reference
- Brett Beauregard's PID blog posts
- NIST SP 800-132 - PBKDF2 recommendations
- NIST CSF - Cybersecurity Framework
- IEC 62443 - Industrial security

---

## License

MIT License - See [LICENSE](LICENSE)

## Author

Ola Andersson

---

## Project Location

**GitHub:** https://github.com/itoaa/adaptivepwm

Note: Project moved from workspace root to projects/ directory.
