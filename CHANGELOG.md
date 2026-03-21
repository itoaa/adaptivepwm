# AdaptivePWM Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [2.1.1] - 2026-03-22

### Security & Safety Framework v1.1.0

#### Added (Framework Enhancement)
- **Project Overview:** Added Section 0 with complete project context, safety justification, and risk analysis
- **Threat Model:** Created `docs/security/threat-model.md` with STRIDE analysis
- **New Security Requirements:**
  - SR-006: PWM rate-of-change limiting (±5%/10ms)
  - SR-007: Thermal runaway protection (dT/dt monitoring)
  - SR-008: Over-current/short-circuit detection
- **MISRA Deviation Register:** Appendix D for approved deviations
- **Approved Tools List:** Appendix E with version pinning

#### Changed (Standards Alignment)
- **Framework Title:** Changed to "Security & Safety Framework (IEC 61508 + IEC 62443 aligned)"
- **Standards:** Updated from CISSP-aligned to IEC 61508 SIL 2 + IEC 62443 SL-2 + CERT C
- **CISSP:** Now referenced as "inspiration" rather than compliance
- **Risk Justification:** Added Section 0.4 explaining all safety thresholds

#### Added (Developer Experience)
- **Code Formatting:** `.clang-format` with MISRA-compatible style
- **Editor Config:** `.editorconfig` for consistent formatting
- **Config Directory:** `config/` for platform-specific settings
- **LICENSE:** MIT License with safety-critical disclaimer

#### Security Impact
- **Level:** Medium
- **Description:** Enhanced threat modeling and safety requirements
- **Justification:** Improves traceability and audit readiness for SIL 2

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

---

## Security Advisory

### [SEC-2026-03-22-001] Framework Enhancement
**Severity:** Medium  
**Description:** Added comprehensive threat modeling and safety requirements to support SIL 2 alignment.  
**Fix:** Implemented STRIDE threat model, new SR requirements, and deviation register.  
**CVSS:** 4.3 (AV:L/AC:L/PR:N/UI:N/S:U/C:N/I:L/A:L)

### [SEC-2026-03-21-001] Clock System Hardening
**Severity:** Medium  
**Description:** Previous clock configuration did not use CSS or optimized clock settings.  
**Fix:** Implemented full clock security with HSE monitoring and optimized PLL configuration.  
**CVSS:** 4.3 (AV:L/AC:L/PR:N/UI:N/S:U/C:N/I:L/A:L)

---

## Compliance Notes

### IEC 61508 / IEC 62443 Framework Alignment
- **IEC 61508:** SIL 2 concepts applied (partial compliance)
- **IEC 62443:** SL-2 security level
- **CERT C:** Secure coding standard
- **MISRA-C:2012:** Partial compliance with deviation register

### Standards Compliance
- IEC 61508 (functional safety) - concepts
- IEC 62443 (industrial security) - SL-2
- CERT C (secure coding)
- MISRA-C:2012 (coding standard) - with deviations

---

## Migration Guide

### From v2.1.0 to v2.1.1

1. **Framework:** Review updated PROJECT_FRAMEWORK.md Section 0
2. **Threat Model:** Read docs/security/threat-model.md
3. **Code Style:** Install .clang-format in your editor
4. **Tools:** Verify toolchain versions match Appendix E

### From v2.0.0 to v2.1.x

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
- ✓ Threat modeling added (v2.1.1)
- ✓ MISRA deviation register added (v2.1.1)

---

## Contributors

- Ola Andersson - Project Owner
- Assistant - Framework Development

---

## References

- [Project Framework](PROJECT_FRAMEWORK.md)
- [Security Documentation](docs/safety.md)
- [Threat Model](docs/security/threat-model.md)
- [Clock Design](docs/design.md)
