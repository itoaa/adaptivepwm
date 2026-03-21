# AdaptivePWM Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [2.1.0] - 2026-03-21

### Security (CISSP-Aligned)
- **Clock Security:** Implemented CSS (Clock Security System) for HSE failure detection
- **Watchdog:** Configured Independent Watchdog (IWDG) with 500ms timeout
- **Boundary Validation:** Added hard limits for PWM duty cycle (2%-98%)

### Changed (Clock System Optimization)
- **HSE:** Changed from default to 16 MHz external crystal
- **PLL:** Configured M=16, N=336, P=4, Q=7 for 84 MHz SYSCLK
- **ADC Clock:** Optimized to 42 MHz (PCLK2/2, maximum allowed)
- **PWM Clock:** Full 84 MHz resolution on APB2
- **Sampling:** Optimized ADC sampling times per channel (3/15/28 cycles)

### Documentation
- **Added:** Complete clock system design document (docs/design.md)
- **Added:** Updated API documentation with clock functions
- **Added:** Security protocols documentation (docs/safety.md)
- **Updated:** Project framework and governance documentation

### Performance
- **ADC:** 2× faster clock, 9× faster sampling for voltage channels
- **Latency:** <50 µs ADC to PWM response time
- **Resolution:** 4200 steps for PWM (12-bit equivalent)

---

## [2.0.0] - 2026-02-27

### Added
- **FreeRTOS:** Real-time operating system integration
- **PWM Driver:** TIM1 complementary PWM with dead-time
- **ADC Driver:** 4-channel DMA-based sampling
- **CLI Interface:** Command-line interface via UART
- **Safety Systems:** Temperature monitoring, overcurrent protection
- **Flash Logger:** Persistent data logging

### Features
- **Control Loop:** 100 Hz efficiency optimization
- **Parameter Calculation:** Real-time L/C/ESR calculation
- **Error Handling:** Multi-level error management
- **Watchdog:** Basic watchdog implementation

---

## [1.0.0] - 2026-02-20

### Added
- Initial project structure
- Basic HAL abstraction layer
- CMake/PlatformIO build system
- Unit test framework

---

## Security Advisory

### [SEC-2026-03-21-001] Clock System Hardening
**Severity:** Medium  
**Description:** Previous clock configuration did not use CSS or optimized clock settings.  
**Fix:** Implemented full clock security with HSE monitoring and optimized PLL configuration.  
**CVSS:** 4.3 (AV:L/AC:L/PR:N/UI:N/S:U/C:N/I:L/A:L)

---

## Compliance Notes

### CISSP Framework Alignment
- Domain 3 (Security Architecture): Clock CSS, Watchdog
- Domain 8 (Software Development): Input validation, fail-safes

### Standards Compliance
- MISRA-C:2012 (partial)
- IEC 61508 concepts (safety)

---

## Migration Guide

### From v2.0.0 to v2.1.0

1. **Hardware:** Ensure 16 MHz HSE crystal is installed
2. **Clock:** Review new clock configuration in `main.c`
3. **Documentation:** Update project documentation per framework requirements
4. **Testing:** Run full test suite with new clock settings

---

## Known Issues

### Open
- None

### Resolved
- ✓ Clock optimization completed (v2.1.0)
- ✓ Documentation framework implemented (v2.1.0)

---

## Contributors

- Ola Andersson - Project Owner
- Assistant - Framework Development

---

## References

- [Project Framework](PROJECT_FRAMEWORK.md)
- [Security Documentation](docs/safety.md)
- [Clock Design](docs/design.md)
