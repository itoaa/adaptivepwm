# AdaptivePWM Project Framework

## Security & Safety Framework (IEC 61508 + IEC 62443 aligned)

**Version:** 1.1.0  
**Date:** 2026-03-22  
**Classification:** Internal  
**Framework Owner:** Ola Andersson  
**Security Compliance:** IEC 61508 (SIL 2), IEC 62443, CERT C, MISRA-C:2012  
**Inspiration:** CISSP principles (Domain 3, 8)

---

## 0. PROJECT OVERVIEW

### 0.1 Project Identity

| Attribute | Value |
|-----------|-------|
| **Name** | AdaptivePWM |
| **Version** | 2.1.0 |
| **Purpose** | Adaptive PWM control system for DC/DC buck/boost converters with real-time efficiency optimization, temperature monitoring, and load regulation |
| **Application Domain** | Industrial power electronics, battery management systems, motor control |
| **Criticality Level** | **SIL 2** (IEC 61508) - Risk of overheating/fire upon PWM malfunction |
| **Target Hardware** | STM32F401RE (ARM Cortex-M4 @ 84 MHz) on NUCLEO-F401RE development board |

### 0.2 Safety Context

**Intended Use:**
- Closed-loop control of DC/DC converters (buck, boost, buck-boost)
- Real-time efficiency optimization based on measured electrical parameters
- Industrial and commercial applications with controlled environments

**Foreseen Misuse:**
- Use outside specified voltage/current/temperature ranges
- Operation without proper heatsinking
- Connection to loads exceeding rated capacity
- Use in safety-critical medical or aerospace applications (not SIL 3/4 rated)

**Hazard Analysis:**
| Hazard | Risk | Mitigation |
|--------|------|------------|
| Overheating/fire | High | Temperature monitoring, thermal shutdown, watchdog |
| Overcurrent damage | Medium | Current sensing, hardware limits |
| EMI interference | Medium | Proper PCB layout, shielding requirements |
| Clock failure | Medium | CSS (Clock Security System), watchdog |

### 0.3 Technical Constraints

- **MCU:** STM32F401RE (84 MHz max, 3.3V, -40°C to +85/105°C)
- **PWM Frequency:** 20 kHz (fixed)
- **PWM Resolution:** 4200 steps (12-bit equivalent @ 84 MHz)
- **ADC Resolution:** 12-bit
- **ADC Sampling:** 10 kHz per channel
- **Control Loop:** 100 Hz

### 0.4 Risk Justification for Safety Requirements

| Requirement | Value | Justification |
|-------------|-------|---------------|
| PWM Duty: 2-98% | Prevents 0%/100% which causes complete shutdown or no regulation | Datasheet STM32F401 (min/max duty cycle for complementary PWM) |
| Watchdog: 500 ms timeout | Detects software deadlock within 5x control loop period | Worst-case transient response < 400 ms + margin |
| Watchdog: 100 ms refresh | Ensures responsiveness while avoiding false triggers | 2.5x max expected interrupt latency |
| Temperature: -40°C to 125°C | MCU operating range + sensor range | STM32F401RE datasheet, LM35 sensor specs |
| Rate-of-change: ±5%/10 ms | Prevents thermal shock in power semiconductors | IGBT/MOSFET thermal time constants |

---

## 1. EXECUTIVE SUMMARY

This framework establishes mandatory governance, security, and development practices for the AdaptivePWM project. All contributors must adhere to these standards, which are enforced through automated checks and manual reviews.

**Key Principles:**
- Security by Design (IEC 62443 SL-2)
- Functional Safety (IEC 61508 SIL 2)
- Continuous Documentation
- Change Management Control
- Code Quality Assurance (MISRA-C:2012, CERT C)
- Defense in Depth

---

## 2. PROJECT STRUCTURE & ORGANIZATION

### 2.1 Directory Hierarchy (MANDATORY)

```
AdaptivePWM/
├── docs/                    # Documentation (REQUIRED)
│   ├── architecture/        # System architecture docs
│   ├── api/                 # API reference
│   ├── security/            # Security documentation
│   │   └── threat-model.md   # STRIDE threat analysis
│   ├── safety/              # Safety protocols
│   └── changes/             # Change logs
├── src/                     # Source code
│   ├── hal/                 # Hardware abstraction
│   ├── core/                # Core logic
│   ├── security/            # Security features
│   └── utils/               # Utilities
├── include/                 # Public headers
├── tests/                   # Test suites
│   ├── unit/                # Unit tests
│   ├── integration/         # Integration tests
│   └── security/            # Security tests
├── config/                  # Platform-specific configs
├── tools/                   # Build & dev tools
├── ci/                      # CI/CD configurations
├── .clang-format            # Code formatting rules
├── .editorconfig           # Editor configuration
├── CMakePresets.json       # CMake presets
└── PROJECT_FRAMEWORK.md     # This document
```

### 2.2 File Naming Conventions

| Type | Pattern | Example |
|------|---------|---------|
| Headers | `module_component.h` | `hal_adc.h` |
| Source | `module_component.c` | `hal_adc.c` |
| Tests | `test_module.c` | `test_hal_adc.c` |
| Docs | `topic_name.md` | `clock_system.md` |
| Config | `config_area.h` | `config_clock.h` |

### 2.3 Build & Toolchain (Appendix E)

**Required Tools:**
- CMake: >= 3.25
- arm-none-eabi-gcc: 13.x (pinned version)
- PlatformIO Core: >= 6.0
- Python: >= 3.8
- HAL Drivers: STM32CubeF4 v1.28.0+ (pinned)

**Version Pinning:** All toolchain versions MUST be specified in `tools/approved_tools.md` (Appendix E).

---

## 3. SECURITY & SAFETY FRAMEWORK

### 3.1 Standards Compliance

| Standard | Level | Application |
|----------|-------|-------------|
| IEC 61508 | SIL 2 | Functional safety for PWM control systems |
| IEC 62443 | SL-2 | Industrial automation security |
| ISO/SAE 21434 | Concepts | Cybersecurity (if automotive use) |
| MISRA-C:2012 | Partial | Coding standard (see Appendix D for deviations) |
| CERT C | Full | Secure coding standard |

### 3.2 Security Requirements (MANDATORY)

#### SR-001: Clock System Integrity
- **Requirement:** CSS (Clock Security System) MUST be enabled
- **Verification:** Code review + hardware test
- **Failure:** System MUST enter safe mode
- **Ref:** STM32F401 Reference Manual, RCC section

#### SR-002: Watchdog Implementation
- **Requirement:** Independent watchdog MUST be configured
- **Timeout:** Maximum 500 ms (justification: 5x worst-case control loop response)
- **Refresh:** Every 100 ms maximum (justification: 2.5x max interrupt latency)
- **Verification:** Fault injection testing
- **Failure:** System reset

#### SR-003: Boundary Validation
- **Requirement:** All inputs MUST be validated
- **PWM Duty:** 2% - 98% hard limits (prevents complete shutdown or no regulation)
- **ADC Values:** Physically possible ranges only
- **Temperature:** -40°C to 125°C (MCU operating range)
- **Justification:** See Section 0.4 Risk Justification

#### SR-004: Fail-Safe Defaults
- **Requirement:** Default to SAFE state on any error
- **PWM:** Stopped on initialization failure
- **Output:** Minimum duty on communication loss
- **Verification:** Power-on self-test (POST)

#### SR-005: Secure Boot (Future)
- **Requirement:** Firmware signature verification
- **Target:** v3.0
- **Status:** Planned

#### SR-006: PWM Rate-of-Change Limiting (NEW)
- **Requirement:** PWM duty cycle changes MUST be rate-limited
- **Limit:** Maximum ±5% per 10 ms
- **Justification:** Prevents thermal shock in power semiconductors
- **Verification:** Unit tests, oscilloscope measurement

#### SR-007: Thermal Runaway Protection (NEW)
- **Requirement:** Detect abnormal temperature rise rate (dT/dt)
- **Threshold:** dT/dt > 5°C/s for 2 seconds → Emergency shutdown
- **Justification:** Prevents catastrophic failure modes

#### SR-008: Over-Current / Short-Circuit Detection (NEW)
- **Requirement:** Hardware-level overcurrent detection via ADC
- **Threshold:** 110% of rated current → Immediate PWM shutdown
- **Threshold:** 150% of rated current → Emergency stop + latch

### 3.3 Deviation from Standards (Appendix D)

Any deviation from MISRA-C:2012 MUST be:
1. Documented in Appendix D
2. Approved by Technical Lead
3. Security-impact assessed
4. Reviewed quarterly

---

## 4. DOCUMENTATION REQUIREMENTS

### 4.1 Mandatory Documentation

| Document | Location | Update Trigger | Reviewer |
|----------|----------|----------------|----------|
| Project Overview | Section 0 | Major architecture change | Project Owner |
| Architecture Design | `docs/architecture/` | Design change | Tech Lead |
| Threat Model | `docs/security/threat-model.md` | Security feature | Security Officer |
| API Reference | `docs/api/` | API change | Developer + Reviewer |
| Safety Manual | `docs/safety/` | Safety-critical change | Safety Engineer |
| Change Log | `CHANGELOG.md` | Every PR | CI System |
| Deviation Register | Appendix D | MISRA deviation | Tech Lead |
| Approved Tools | Appendix E | Toolchain change | Tech Lead |

### 4.2 Documentation Standards

#### Code Documentation (Doxygen)
```c
/**
 * @brief Brief description
 * @details Detailed description with context
 * @param[in] param_name Input parameter description
 * @param[out] result Output parameter description
 * @return Description of return value
 * @retval true Success condition
 * @retval false Failure condition
 * @pre Preconditions that must be met
 * @post Postconditions after execution
 * @note Additional notes
 * @warning Warning about usage
 * @security Security considerations (IEC 62443)
 * @safety Safety considerations (IEC 61508)
 * 
 * @code
 * // Usage example
 * bool ok = function_name(param);
 * @endcode
 */
```

#### Threat Modeling (STRIDE)
See `docs/security/threat-model.md` for:
- Data Flow Diagrams (DFD)
- STRIDE-per-element analysis
- Risk scoring (DREAD or CVSS)
- Mitigation mapping

### 4.3 Documentation Review Process

```
Developer Change
      ↓
Update Relevant Documentation
      ↓
Self-Review Checklist (Appendix C)
      ↓
Peer Review (Documentation + Code)
      ↓
[Gate] Documentation Complete?
      ↓
Merge to Main
```

---

## 5. CHANGE MANAGEMENT

### 5.1 Change Classification

| Class | Description | Approval | Documentation | Security Impact |
|-------|-------------|----------|---------------|-----------------|
| Critical | Safety/security impact | Security Officer + Tech Lead | Full | Required in CHANGELOG |
| Major | Feature change | Tech Lead | API + Architecture | Required |
| Minor | Bug fix, optimization | Peer Review | Change Log | Required if applicable |
| Patch | Typo, comments | Self | Commit message | None |

### 5.2 Commit Message Format (with Security Impact)

```
type(scope): subject

body (what and why)

Breaking Changes: (if any)
Security Impact: [None/Low/Medium/High/Critical] - justification
Safety Impact: [None/Low/Medium/High/Critical] - justification
Documentation: (what was updated)
MISRA Deviations: (if any, reference Appendix D)

Refs: #123
```

---

## 6. CODE QUALITY STANDARDS

### 6.1 Standards Stack

| Standard | Compliance | Tool |
|----------|-----------|------|
| MISRA-C:2012 | Required (see Appendix D) | PC-lint Plus |
| CERT C | Required | Cppcheck |
| IEC 61508 | SIL 2 guidance | Manual review |
| IEC 62443 | SL-2 guidance | Manual review |

### 6.2 MISRA-C:2012 Deviation Register (Appendix D)

Any deviation MUST be documented with:
- Rule number and category
- Justification (why deviation is necessary)
- Mitigation (how risk is managed)
- Approval date and approver
- Review date

**Deviation Categories:**
- Advisory (can be ignored with justification)
- Required (MUST be approved by Technical Lead)
- Mandatory (CANNOT be deviated from)

---

## 7. TESTING REQUIREMENTS

### 7.1 Testing Framework (Recommended)

**Unit Testing:**
- Framework: Unity + CMock (via PlatformIO)
- Coverage: >90% for HAL layer, >95% for safety-critical code

**Hardware-in-Loop (HIL):**
- Tools: Renode or QEMU for STM32
- Scope: Integration testing, fault injection

**Security Testing:**
- Static analysis: PC-lint Plus, Cppcheck
- Dynamic analysis: Fault injection (clock, memory, power)
- Fuzz testing: Input validation

---

## 8. APPENDICES

### Appendix A: Security Review Template
See `docs/security/SECURITY_REVIEW_TEMPLATE.md`

### Appendix B: Documentation Update Checklist

```markdown
## Documentation Update Required?

- [ ] Architecture docs updated (if design changed)
- [ ] API reference updated (if interface changed)
- [ ] Security guide updated (if security changed)
- [ ] Safety manual updated (if safety changed)
- [ ] Threat model updated (if attack surface changed)
- [ ] Change log updated (with Security Impact)
- [ ] README updated (if user-visible)
- [ ] Code comments updated
- [ ] Doxygen comments added
```

### Appendix C: Self-Review Checklist

```markdown
## Pre-Commit Review

### Security
- [ ] SR-001: CSS enabled (if HSE used)
- [ ] SR-002: Watchdog configured
- [ ] SR-003: Boundary validation present
- [ ] SR-004: Fail-safe defaults
- [ ] SR-006: Rate-of-change limits (PWM)
- [ ] SR-007: Thermal runaway protection
- [ ] SR-008: Over-current detection
- [ ] No hardcoded secrets
- [ ] Input validation present

### Quality
- [ ] MISRA-C:2012 compliance (or deviation documented)
- [ ] CERT C compliance
- [ ] Unit tests passing
- [ ] Static analysis clean
- [ ] Documentation updated
```

### Appendix D: MISRA Deviation Register

| Rule | Category | Justification | Mitigation | Approved By | Date | Review |
|------|----------|---------------|------------|-------------|------|--------|
| 11.1 | Required | Pointer conversion for HAL | Type checking wrapper | Tech Lead | 2026-03-22 | 2026-06-22 |
| 13.1 | Advisory | Complex init sequence | Reviewed by senior dev | Tech Lead | 2026-03-22 | 2026-06-22 |

**Note:** All deviations MUST be reviewed quarterly.

### Appendix E: Approved Tools List

| Tool | Version | Purpose | Verification |
|------|---------|---------|--------------|
| arm-none-eabi-gcc | 13.2.1 | Compiler | `arm-none-eabi-gcc --version` |
| CMake | 3.28.0 | Build system | `cmake --version` |
| PlatformIO | 6.1.0 | Build/dev | `pio --version` |
| STM32CubeF4 | 1.28.0 | HAL drivers | `cat platformio.ini` |
| PC-lint Plus | 1.4.0 | MISRA checking | N/A (manual) |
| Cppcheck | 2.13 | Static analysis | `cppcheck --version` |
| Python | 3.11.0 | Scripts | `python3 --version` |

**Toolchain Update Process:**
1. Test new version in staging branch
2. Security review if compiler/toolchain
3. Update Appendix E
4. Update CI/CD configurations
5. Notify all developers

---

## 9. ROLES & RESPONSIBILITIES

### 9.1 Current Roles (Early Phase)

**Note:** In early development phases, roles are combined. All role assignments MUST be documented at each review.

| Role | Current Assignment | Responsibilities |
|------|-------------------|------------------|
| Project Owner | Ola Andersson | Framework maintenance, final approval |
| Technical Lead | Ola Andersson | Architecture, code review, MISRA deviations |
| Security Officer | Ola Andersson | Security reviews, threat modeling |
| Safety Engineer | Ola Andersson | Safety case, risk assessment |

**Expansion Plan:** As team grows, roles MUST be separated per IEC 61508 requirements.

---

## 10. ENFORCEMENT & AUTOMATION

### 10.1 Automated Enforcement

**Fast Checks (Pre-commit):**
- Code formatting (`.clang-format`)
- Basic style checks
- File naming conventions

**Full Checks (CI/CD):**
- MISRA-C:2012 compliance
- Full framework check
- Unit test execution
- Static analysis

### 10.2 Branch Strategy

**Current:** Gitflow (feature/develop/main)

**Future:** Consider trunk-based development with feature flags for faster iteration (recommended for solo/small teams).

---

## 11. SBOM & DEPENDENCIES

### 11.1 Software Bill of Materials

Required components:
- HAL: STM32CubeF4 v1.28.0
- RTOS: FreeRTOS (if used)
- Build: PlatformIO, CMake
- Test: Unity/CMock

**Policy:** Third-party code MUST be:
1. Vetted for security vulnerabilities
2. License-compatible (MIT/Apache/BSD preferred)
3. Pinned to specific versions
4. Documented in SBOM

---

## 12. REAL-TIME ANALYSIS

### 12.1 WCET (Worst-Case Execution Time)

**Required Analysis:**
- Control loop: < 10 ms (measured)
- ADC interrupt: < 100 µs (measured)
- PWM update: < 1 µs (measured)

**Schedulability:** FreeRTOS task analysis required if RTOS used.

---

**Document Control**

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.0 | 2026-03-21 | Assistant | Initial framework |
| 1.1.0 | 2026-03-22 | Assistant | Added Project Overview, IEC 62443, new SR requirements, Appendix D/E, MISRA deviations, threat modeling |

**Next Review:** 2026-06-22  
**Approval:**

Project Owner: Ola Andersson Date: 2026-03-22
