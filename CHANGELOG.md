# AdaptivePWM Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [2.3.1] - 2026-04-15

### Security (SEC-033: Hardware RNG Integration)

#### Added
- **STM32F401 Hardware RNG Integration:** Replaced LCG-based software PRNG with hardware True Random Number Generator
  - Uses dedicated entropy source from analog noise (ring oscillators)
  - Provides cryptographically secure random numbers for salt generation
  - NIST SP 800-90B compliant on STM32F4 series
  - ~40x faster than software PRNG (~2.5µs vs ~100µs for 16 bytes)
  - Immune to PRNG seed prediction attacks
- **RNG Configuration Options:** Added comprehensive RNG configuration in `src/config.h`
  - `RNG_ENABLED`: Enable hardware RNG (default: 1)
  - `RNG_TIMEOUT_MS`: Timeout for RNG operations (default: 100ms)
  - `RNG_MAX_ATTEMPTS`: Maximum retry attempts (default: 3)
  - `RNG_ERROR_RECOVERY`: Auto-recovery from clock/seed errors (default: 1)
  - `RNG_FALLBACK_SOFTWARE`: Software fallback (default: 0 - DISABLED for security)
- **Error Handling:** Comprehensive error detection and recovery
  - Clock error detection (CECS) with automatic recovery
  - Seed error detection (SECS) with retry mechanism
  - Timeout protection for stalled RNG operations
- **Hardware RNG Documentation:** Created `docs/security/RNG_IMPLEMENTATION.md`
  - Complete architecture description
  - Register-level details
  - Security analysis and comparisons
  - Testing procedures
- **Feature Flag:** Added `FEATURE_HARDWARE_RNG` to security feature flags
- **RNG Unit Tests:** Added hardware RNG validation tests
  - `test_Auth_HardwareRNG_ShouldBeEnabled()`
  - `test_Auth_HardwareRNG_Configuration()`
  - `test_Auth_PasswordSalt_ShouldBeRandom()`
  - `test_Auth_RNG_ShouldGenerateDifferentValues()`
  - `test_Auth_RNG_NoFallbackInProduction()`

#### Changed
- **`secure_random()` function:** Completely rewritten to use hardware RNG
  - Previous: LCG PRNG seeded from `HAL_GetTick()` (predictable)
  - New: STM32F401 hardware RNG via HAL (`HAL_RNG_GenerateRandomNumber()`)
  - Maintains same function signature for backward compatibility
- **`CLI_Auth_Init()`:** Now initializes hardware RNG during startup
- **`generate_salt()`:** Now uses hardware RNG for all salt generation
- **Security Framework Updates:** Added CISSP Domain 6 and ISO 27001 A.8.24 references
- **Version bump:** 2.3.0 → 2.3.1 (security patch)

#### Deprecated
- **Software PRNG:** LCG-based random number generation is deprecated
  - Only available if `RNG_FALLBACK_SOFTWARE` is explicitly enabled
  - NOT RECOMMENDED for production use

#### Security Impact
- **Vulnerability Addressed:** ADP-TEST-001 (CVSS 5.9 - MEDIUM)
  - Issue: Predictable PRNG seeded from system tick
  - Fix: True hardware RNG with analog entropy source
- **Attack Surface:** Eliminated PRNG seed prediction attacks
- **Entropy Quality:** Hardware RNG provides true randomness vs pseudo-randomness
- **Compliance:** Now meets NIST SP 800-90B requirements

#### Security Framework Alignment
- **CISSP Domain 5:** Identity and Access Management (IAM) - Secure authentication
- **CISSP Domain 6:** Security Assessment and Testing - Cryptographically secure RNG
- **NIST CSF 2.0:** PR.DS-02 (Data security with cryptographic protection)
- **ISO 27001:2022:** A.8.24 (Use of cryptography), A.8.5 (Secure authentication)

#### Performance Impact
- **Salt Generation:** ~2.5µs (was ~100µs) - 40x improvement
- **PBKDF2 Total Time:** Unchanged (~250ms, dominated by iterations)
- **Memory Usage:** +24 bytes for RNG handle
- **Code Size:** +1.2KB for RNG driver and error handling

#### Files Modified
- `src/config.h` - Added RNG configuration section
- `src/cli_auth.c` - Implemented hardware RNG in `secure_random()`
- `src/cli_auth.h` - Added security framework documentation
- `tests/test_cli_auth.c` - Added hardware RNG unit tests
- `docs/security/RNG_IMPLEMENTATION.md` - New documentation

---

## [2.3.1] - 2026-04-13

### Security (SEC-027: PBKDF2 Iteration Increase)

#### Added
- **NIST SP 800-132 Compliance:** PBKDF2-SHA256 iteration count increased from 1,000 to **100,000**
  - Provides ~100x increased resistance to brute-force attacks
  - Authentication latency remains acceptable (~200-300ms on STM32F4 @ 84 MHz)
  - Added compile-time check with warning for iteration counts below 100,000
- **Performance Benchmark Tests:** Added unit test for hash computation timing verification
- **Security Documentation:** Updated `docs/security/uart-authentication.md` with SEC-027 details

#### Changed
- **CLI_AUTH_HASH_ITERATIONS:** Updated from 1000 to 100000 in `src/config.h`
- **Documentation:** Added NIST SP 800-132 references and compliance notes
- **Unit Tests:** Added `test_Auth_HashIterations_ShouldBeNISTCompliant()` and `test_Auth_HashComputation_ShouldCompleteWithinTime()`

#### Security Framework Alignment
- **CISSP Domain 3:** Security Architecture and Engineering
- **NIST CSF 2.0:** PR.DS-06 (Data protection)
- **ISO 27001:2022:** A.9.4 (Access control)

#### Performance Impact
- **Hash Computation Time:** ~200-300ms on STM32F4 @ 84 MHz
- **Target Compliance:** <500ms authentication latency (met)
- **Brute-force Resistance:** ~100x improvement over 1,000 iterations
- **Stack Usage:** Unchanged (~512 bytes during hash computation)

---

## [2.2.1] - 2026-04-10

### Improved (Control Systems)

#### PID Controller Enhancements
- **Setpoint Weighting:** Added configurable setpoint weighting (0-1) to reduce overshoot while maintaining response speed
- **Derivative on Measurement:** Changed from derivative on error to derivative on measurement - prevents derivative kick on setpoint changes
- **Derivative Filtering:** Added low-pass filter for derivative term (configurable alpha 0-1)
- **Back-calculation Anti-Windup:** Enhanced integral windup prevention with conditional back-calculation only when saturated
- **Runtime Gain Adjustment:** Added `PID_SetGains()`, `PID_SetSetpointWeight()`, `PID_SetDerivativeFilter()` functions
- **Integral Access:** Added `PID_GetIntegral()` and `PID_SetIntegral()` for manual/auto bumpless transfer

#### PWM Improvements
- **Setpoint Ramping:** Added configurable rate limiting (PWM_RAMP_RATE_PER_SEC) for smooth duty cycle transitions
- **Feedforward Control:** Added buck/boost converter feedforward calculation (`Adaptive_PWM_CalculateFeedforward()`)
- **Immediate Mode:** Added `Adaptive_PWM_SetDutyImmediate()` for emergency responses bypassing ramp
- **Ramping Status:** Added `Adaptive_PWM_IsRamping()` and `Adaptive_PWM_GetTargetDuty()` for monitoring
- **Runtime Ramp Rate:** Added `Adaptive_PWM_SetRampRate()` for dynamic adjustment

#### ADC Enhancements
- **Dual-Stage Filtering:** Combined IIR and moving average filters for superior noise rejection
- **Moving Average:** Added 8-sample moving average as first stage (configurable size)
- **Transient Detection:** Added automatic transient detection with threshold (ADC_TRANSIENT_THRESHOLD)
- **Adaptive Sampling:** Framework for increasing sampling rate during transients (ADC_FAST_SAMPLE_RATE_HZ)
- **Channel-Specific Sampling:** Optimized sampling times per channel (3/15/28 cycles)
- **Better Calibration:** Improved calibration infrastructure with gain/offset per channel

### Changed
- **Ki from 0.0 to 0.01:** Now using integral term in PID for steady-state error correction
- **Kd from 0.0 to 0.001:** Added derivative term for better transient response
- **Config Macro Renames:** `MEASUREMENT_ALPHA` → `ADC_FILTER_IIR_ALPHA` for clarity
- **Flash Usage:** 29800 bytes (+128 bytes from v2.1.1)
- **RAM Usage:** 6740 bytes (+132 bytes from v2.1.1)

### Fixed
- **Transient Detection:** Now properly detects and flags transient conditions for higher sampling rates
- **Derivative Calculation:** Fixed derivative-on-measurement implementation (was using simplified version)
- **Moving Average Index:** Fixed bug where moving average index wasn't updated correctly

### Documentation
- Updated function headers with new parameters
- Added inline documentation for feedforward calculation
- Documented dual-filter architecture

### Performance Impact
- **Filter Latency:** ~8ms additional latency from moving average (acceptable for control loop)
- **Transient Response:** <100ms detection and adaptation to transients
- **Noise Rejection:** ~6dB improvement from dual filtering

---

## [2.1.1] - 2026-03-22

### Security & Safety Framework v1.1.0

#### Added (Framework Enhancement)
- **Project Overview:** Added Section 0 with complete project context, safety justification, and risk analysis
- **Threat Model:** Created `docs/security/threat-model.md` with STRIDE analysis
- **New Security Requirements:**
  - SR-006: PWM rate-of-change limiting (±5%/10ms)
  - SR-007: Thermal runaway protection (dT/dt monitoring)
- **Safety Improvements:**
  - Safety interlock with physical button for critical parameter changes
  - Differential monitoring between redundant temperature sensors
  - Improved risk assessment with ISO 13849 PL calculations

### Security (Framework Implementation)

#### Added
- **Flash Logger HMAC-SHA256 Integrity Protection (SEC-023):**
  - Every flash log entry signed with HMAC-SHA256 (32 bytes)
  - Cryptographic chaining - each entry includes hash of previous entry
  - Tamper-evident log structure prevents undetected modification
  - Key storage in dedicated flash sector (0x080D0000) with CRC protection
  - Salt (16 bytes) added per entry for additional security
  - Compile-time selectable: FLASH_LOGGER_HMAC_ENABLED
  - Magic values: FLASH_LOG_HMAC_MAGIC (0x484D4143), FLASH_LOG_CHAIN_MAGIC (0x43484131)
  - Entry size: 80 bytes (was 32) to accommodate signature + salt
  - Halved capacity but provides cryptographic integrity guarantees
  - **Security Framework:** CISSP Domain 3, NIST PR.DS-06, ISO 27001 A.12.4

- **Security Requirements:** Added framework version tracking in PROJECT_FRAMEWORK.md

#### Changed
- **Flash Logger Structure:** Increased from 32 to 80 bytes per entry
- **Log Capacity:** Reduced from 256 to 128 entries (security trade-off)
- **Version:** Framework v1.1.0 (from v1.0.1)

#### Security Impact
- **Log Tampering:** Now cryptographically detectable
- **Key Protection:** Dedicated sector with integrity check
- **Chain Verification:** Full chain validation from any entry
- **Performance:** Minimal impact (~5µs additional per log entry)
- **Flash Wear:** Increased write amplification due to larger entries

---

## [2.1.0] - 2026-03-21

### Major Architecture Improvement

#### Complete PID Controller Rewrite (Adaptive Control Enhancement)

**Motivation:** The previous implementation used basic PID with fixed gains. This version introduces a complete adaptive control system that self-tunes based on system response and operating conditions.

#### Added
- **Adaptive Gain Scheduling:**
  - Gains automatically adjust based on operating point (efficiency, load, temperature)
  - `Adaptive_Control_UpdateGains()` - recalculates optimal gains every 100ms
  - Maintains stability margins while maximizing performance
  
- **Anti-Windup Protection:**
  - Conditional integration: Integrator only updates when not saturated
  - Prevents integrator windup during startup or large setpoint changes
  - Faster recovery from saturation

- **Efficiency Tracking:**
  - Real-time efficiency calculation: `Adaptive_Control_GetEfficiency()`
  - Efficiency-based gain adjustment for optimal performance
  - Moving average filter on efficiency (8 samples)

- **Safety Limits:**
  - Configurable min/max duty cycle with hysteresis
  - Temperature-based derating: `Adaptive_Control_DerateIfNeeded()`
  - Configurable limits via `SetMinMaxDutyCycle()` and `SetTempDeratingConfig()`

- **Duty Cycle Smoothing:**
  - Optional smoothing filter (configurable: SMOOTHING_FACTOR)
  - Prevents rapid duty cycle changes that could stress components
  - Time constant: ~50ms at 1kHz control rate

#### Changed
- **PID Controller Structure:** Complete rewrite with new fields
  - Added efficiency, load, temperature tracking
  - Anti-windup implementation
  - Adaptive gain scheduling state
- **Control Loop:** Now calls `Adaptive_Control_Update()` for continuous optimization
- **Duty Cycle Updates:** Now go through adaptive control system
- **Flash Usage:** 29572 bytes (increase of ~800 bytes)
- **RAM Usage:** 6560 bytes (increase of ~80 bytes)

#### Removed
- Fixed PID gains from config.h (now calculated dynamically)
- Simple duty cycle limiting (replaced with comprehensive safety limits)

#### API Additions
```c
// New functions
float Adaptive_Control_GetEfficiency(void);
float Adaptive_Control_GetLoadFactor(void);
float Adaptive_Control_GetFilteredTemperature(void);
void Adaptive_Control_SetMinMaxDutyCycle(float min, float max);
void Adaptive_Control_SetTempDeratingConfig(float warning, float critical, float shutdown);
void Adaptive_Control_DerateIfNeeded(float temp);

// Modified functions
bool Adaptive_Control_Update(float setpoint, float measured_temp, 
                              float measured_voltage, float measured_current,
                              uint32_t dt_ms);
float Adaptive_Control_GetDutyCycle(void);  // Now returns smoothed value

// New initialization
void Adaptive_Control_InitEx(float Kp, float Ki, float Kd, 
                            float output_min, float output_max,
                            float efficiency_threshold);
```

#### Performance Impact
- **Control Loop:** +12µs execution time (now ~45µs)
- **Adaptation:** Runs at 10Hz (every 100ms), adds ~200µs
- **Smoothing:** Filter adds 1ms latency (acceptable for thermal control)
- **Memory:** +80 bytes RAM, +800 bytes Flash

### Documentation
- Updated PROJECT_FRAMEWORK.md with new adaptive control details
- Updated QUICK_REFERENCE.md with new API functions
- Added inline documentation for all new functions

---

## [2.0.1] - 2026-03-21

### Security & Documentation

#### Added
- **Security Framework:** `docs/security/SECURITY_REVIEW_TEMPLATE.md`
- **Threat Model:** `docs/security/threat-model.md` with STRIDE analysis
- **Safety Requirements:** `SAFETY_REQUIREMENTS.md` (IEC 60730 / UL 60730 compliance)

#### Security (SEC-019: UART Authentication)
- **CLI Authentication Module:** `src/cli_auth.c` / `src/cli_auth.h`
  - PBKDF2-SHA256 password hashing with per-password random salt
  - Secure credential storage in flash with CRC integrity
  - Account lockout after 3 failed attempts (5-minute duration)
  - Configurable session timeout (default: 5 minutes)
  - Password strength enforcement (min 4 chars, alphanumeric)
- **Security Framework:** CISSP Domain 4, NIST PR.AC-01, ISO 27001 A.8.5

#### Safety (IEC 60730 Class B)
- Class B software requirements checklist
- Watchdog timer configuration (500ms timeout)
- Flash-based event logging with wear leveling
- Safety critical function classification

#### Changed
- Version: 2.0.1 (security enhancement patch)
- Flash Usage: 28500 bytes (+2100 bytes for auth + logging)
- RAM Usage: 6480 bytes (+180 bytes)

---

## [2.0.0] - 2026-02-28

### Initial Stable Release

#### Added
- **Adaptive PWM Control:** Complete buck/boost converter control system
  - Temperature-based adaptive control
  - PID control with anti-windup
  - Multi-channel ADC with DMA
  - UART CLI for configuration
- **Hardware Support:**
  - STM32F401RE microcontroller
  - TIM1 for high-resolution PWM
  - ADC1 with 4 channels
  - USART2 for CLI communication
- **Documentation:**
  - PROJECT_FRAMEWORK.md (v1.0.0)
  - QUICK_REFERENCE.md
  - SETUP_GUIDE.md
  - EVALUATION.md
- **Testing:**
  - Unity test framework
  - Unit tests for core functions
- **Build System:**
  - Makefile support
  - PlatformIO configuration

#### Technical Specifications
- **Clock:** 84 MHz system clock from 16 MHz HSE
- **PWM:** 20 kHz switching frequency, 12-bit resolution
- **ADC:** 10 kHz sampling, 12-bit resolution, DMA transfer
- **UART:** 115200 baud, interrupt-driven
- **Flash Usage:** ~26400 bytes
- **RAM Usage:** ~6300 bytes

---

## Version History

| Version | Date | Type | Key Changes |
|---------|------|------|-------------|
| 2.3.1 | 2026-04-15 | Security | Hardware RNG (SEC-033) |
| 2.3.1 | 2026-04-13 | Security | PBKDF2 iterations (SEC-027) |
| 2.2.1 | 2026-04-10 | Feature | PID enhancements, PWM feedforward |
| 2.1.1 | 2026-03-22 | Security | HMAC-SHA256 flash logger (SEC-023) |
| 2.1.0 | 2026-03-21 | Feature | Adaptive PID controller |
| 2.0.1 | 2026-03-21 | Security | UART authentication (SEC-019) |
| 2.0.0 | 2026-02-28 | Release | Initial stable release |

---

**Classification:** Public  
**Last Updated:** 2026-04-15  
**Maintainer:** AdaptivePWM Team
