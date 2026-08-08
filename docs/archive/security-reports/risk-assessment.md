# AdaptivePWM Formal Risk Assessment

**Document ID:** ADP-RISK-001  
**Version:** 1.0.0  
**Date:** 2026-04-18  
**Classification:** Security Documentation  
**Framework:** CISSP Domain 1, NIST CSF GOVERN, ISO 27001 A.5.36

---

## 1. Executive Summary

This document presents a formal risk assessment for the AdaptivePWM embedded motor controller project. The assessment follows established security frameworks (CISSP, NIST CSF, ISO 27001) to identify, analyze, and evaluate risks across hardware, software, operational, and supply chain domains.

### Key Findings

| Risk Category | Total Risks | Critical | High | Medium | Low |
|---------------|-------------|----------|------|--------|-----|
| Hardware | 8 | 1 | 2 | 3 | 2 |
| Software | 12 | 2 | 4 | 4 | 2 |
| Operational | 6 | 0 | 2 | 3 | 1 |
| Supply Chain | 4 | 1 | 1 | 1 | 1 |
| **Total** | **30** | **4** | **9** | **11** | **6** |

### Risk Treatment Summary

| Treatment | Count | Percentage |
|-----------|-------|------------|
| Mitigate | 18 | 60% |
| Transfer | 4 | 13% |
| Accept | 6 | 20% |
| Avoid | 2 | 7% |

---

## 2. Scope and Context

### 2.1 System Description

AdaptivePWM is a real-time embedded control system for buck/boost converters and electronic speed controllers (ESCs) built on the STM32F401RE microcontroller platform.

### 2.2 Assessment Scope

**In Scope:**
- STM32F401RE hardware platform
- Firmware codebase (bootloader + application)
- Development and build toolchain
- Production flashing process
- Supply chain components

**Out of Scope:**
- External cloud services
- End-user applications
- Third-party integration (unless using our libraries)

### 2.3 Risk Assessment Methodology

This assessment uses a **qualitative risk analysis** approach based on:

| Component | Standard/Framework |
|-----------|-------------------|
| Risk Identification | ISO 27005, NIST SP 800-30 |
| Threat Modeling | STRIDE methodology |
| Risk Scoring | Likelihood × Impact matrix |
| Treatment Selection | ISO 27001 risk treatment guidelines |

### 2.4 Risk Scoring Matrix

**Likelihood Scale:**

| Level | Score | Description |
|-------|-------|-------------|
| Rare | 1 | Occurs once in >5 years |
| Unlikely | 2 | Occurs once in 2-5 years |
| Possible | 3 | Occurs once in 1-2 years |
| Likely | 4 | Occurs several times per year |
| Almost Certain | 5 | Occurs frequently |

**Impact Scale:**

| Level | Score | Safety | Financial | Reputation |
|-------|-------|--------|-----------|------------|
| Negligible | 1 | No injury | <$1K | None |
| Minor | 2 | First aid | $1K-$10K | Internal notice |
| Moderate | 3 | Medical treatment | $10K-$100K | External notice |
| Major | 4 | Serious injury | $100K-$1M | Media coverage |
| Catastrophic | 5 | Death | >$1M | Regulatory action |

**Risk Score = Likelihood × Impact**

| Score | Risk Level | Action Required |
|-------|------------|-----------------|
| 1-4 | Low | Monitor |
| 5-9 | Medium | Plan mitigation |
| 10-16 | High | Implement mitigation |
| 17-25 | Critical | Immediate action |

---

## 3. Asset Inventory

### 3.1 Hardware Assets

| Asset ID | Asset Name | Description | Owner | Classification |
|----------|------------|-------------|-------|----------------|
| HW-001 | STM32F401RE | Main microcontroller | Project | Critical |
| HW-002 | Flash Memory | 512KB internal flash | Project | Critical |
| HW-003 | SRAM | 96KB internal RAM | Project | Critical |
| HW-004 | ADC Peripheral | Analog-to-digital converter | Project | High |
| HW-005 | TIM1 Peripheral | Advanced PWM timer | Project | Critical |
| HW-006 | UART Peripherals | Serial communication | Project | Medium |
| HW-007 | Debug Interface | SWD/JTAG interface | Project | High |
| HW-008 | Development Boards | Nucleo-F401RE boards | Project | Medium |
| HW-009 | Production Programmers | ST-Link/V2 devices | Project | High |
| HW-010 | Test Equipment | Oscilloscopes, multimeters | Project | Low |

### 3.2 Software Assets

| Asset ID | Asset Name | Description | Owner | Classification |
|----------|------------|-------------|-------|----------------|
| SW-001 | Bootloader | Secure boot firmware | Project | Critical |
| SW-002 | Application Firmware | Main control firmware | Project | Critical |
| SW-003 | HAL Library | STM32 Hardware Abstraction Layer | ST Micro | High |
| SW-004 | CMSIS | Cortex Microcontroller Software Interface | ARM | High |
| SW-005 | Crypto Libraries | Ed25519, SHA-256 implementations | Project/3rd party | Critical |
| SW-006 | Build Scripts | Makefiles, build automation | Project | Medium |
| SW-007 | Test Suite | Unit and integration tests | Project | High |
| SW-008 | Signing Tools | Firmware signing utilities | Project | Critical |
| SW-009 | CLI Tools | Command-line utilities | Project | Medium |
| SW-010 | Documentation | Technical documentation | Project | Medium |

### 3.3 Data Assets

| Asset ID | Asset Name | Description | Sensitivity | Classification |
|----------|------------|-------------|-------------|----------------|
| DATA-001 | Private Keys | Ed25519 signing keys | Critical | Critical |
| DATA-002 | Configuration Data | Device parameters | High | High |
| DATA-003 | Log Data | Runtime logs and telemetry | Medium | Medium |
| DATA-004 | Calibration Data | ADC/PWM calibration values | High | High |
| DATA-005 | Password Hashes | User authentication data | Critical | Critical |
| DATA-006 | Firmware Images | Binary firmware files | High | High |
| DATA-007 | Source Code | Project source repository | High | High |

### 3.4 Documentation Assets

| Asset ID | Asset Name | Description | Classification |
|----------|------------|-------------|----------------|
| DOC-001 | Architecture Design | System architecture documents | High |
| DOC-002 | API Documentation | Public interface documentation | Medium |
| DOC-003 | Security Documentation | Security procedures and guides | Critical |
| DOC-004 | Threat Model | System threat model | High |
| DOC-005 | Test Reports | Security and safety test results | High |

---

## 4. Threat Assessment

### 4.1 Threat Categories

#### 4.1.1 Physical Access Threats

| Threat ID | Threat Description | Threat Actor | Motivation |
|-----------|-------------------|--------------|------------|
| T-PHY-001 | Physical tampering with device | Attacker with device access | Reverse engineering, extraction |
| T-PHY-002 | JTAG/SWD debug interface exploitation | Sophisticated attacker | Firmware extraction, modification |
| T-PHY-003 | Side-channel power analysis | Advanced attacker | Cryptographic key extraction |
| T-PHY-004 | Glitching attacks (voltage/clock) | Advanced attacker | Bypass security checks |
| T-PHY-005 | Cold boot memory extraction | Sophisticated attacker | Extract RAM contents |

#### 4.1.2 Network/Communication Threats

| Threat ID | Threat Description | Threat Actor | Motivation |
|-----------|-------------------|--------------|------------|
| T-NET-001 | UART command injection | Local attacker | Unauthorized control |
| T-NET-002 | Firmware update interception | Man-in-the-middle | Malicious firmware injection |
| T-NET-003 | Replay attacks on commands | Network attacker | Repeat valid commands |
| T-NET-004 | Protocol fuzzing | Automated attacker | Find vulnerabilities |

#### 4.1.3 Firmware Modification Threats

| Threat ID | Threat Description | Threat Actor | Motivation |
|-----------|-------------------|--------------|------------|
| T-FW-001 | Unauthorized firmware flashing | Attacker with physical access | Install malicious firmware |
| T-FW-002 | Bootloader bypass | Sophisticated attacker | Execute arbitrary code |
| T-FW-003 | Firmware rollback to vulnerable version | Sophisticated attacker | Exploit known vulnerabilities |
| T-FW-004 | Malicious firmware signing key compromise | Advanced persistent threat | Sign malicious firmware |
| T-FW-005 | Supply chain firmware substitution | Supply chain attacker | Pre-installed backdoors |

#### 4.1.4 Side-Channel Threats

| Threat ID | Threat Description | Threat Actor | Motivation |
|-----------|-------------------|--------------|------------|
| T-SC-001 | Timing analysis on authentication | Sophisticated attacker | Password/key extraction |
| T-SC-002 | Power consumption analysis | Advanced attacker | Algorithm/key extraction |
| T-SC-003 | Electromagnetic emanation analysis | Advanced attacker | Data extraction |
| T-SC-004 | Acoustic side-channel | Research-level attacker | Extract operations/data |

#### 4.1.5 Supply Chain Threats

| Threat ID | Threat Description | Threat Actor | Motivation |
|-----------|-------------------|--------------|------------|
| T-SUP-001 | Compromised microcontroller chips | Nation-state, organized crime | Hardware backdoors |
| T-SUP-002 | Malicious development tools | Supply chain attacker | Trojanized build outputs |
| T-SUP-003 | Compromised dependency libraries | Open source attacker | Backdoored libraries |
| T-SUP-004 | Insider threat during manufacturing | Disgruntled employee | Intentional defects |

### 4.2 Threat Actor Profiling

| Actor | Capability | Intent | Opportunity |
|-------|------------|--------|-------------|
| Script Kiddie | Low | Low | High |
| Hobbyist Attacker | Medium | Low | Medium |
| Organized Criminal | High | Medium | Low |
| Nation-State | Very High | High | Low |
| Insider | Medium-High | Variable | Very High |

---

## 5. Vulnerability Assessment

### 5.1 Hardware Vulnerabilities

| Vuln ID | Description | Affected Assets | CVSS 3.1 |
|---------|-------------|-----------------|----------|
| V-HW-001 | RDP Level 0 allows flash readout | HW-002, HW-007 | 7.1 (High) |
| V-HW-002 | Debug interface enabled in production | HW-007 | 6.8 (Medium) |
| V-HW-003 | No tamper detection mechanism | HW-001 | 4.4 (Medium) |
| V-HW-004 | Side-channel leakage in crypto operations | HW-001 | 5.9 (Medium) |

### 5.2 Software Vulnerabilities

| Vuln ID | Description | Affected Assets | CVSS 3.1 |
|---------|-------------|-----------------|----------|
| V-SW-001 | Buffer overflow in CLI input handling | SW-009 | 8.1 (High) |
| V-SW-002 | Timing side-channel in password comparison | SW-001 | 5.3 (Medium) |
| V-SW-003 | Insufficient PBKDF2 iterations | SW-005 | 5.9 (Medium) |
| V-SW-004 | Missing input validation on configuration | SW-002 | 6.5 (Medium) |
| V-SW-005 | Integer overflow in size calculations | SW-001 | 7.5 (High) |
| V-SW-006 | Race condition in flash write operations | SW-001 | 5.4 (Medium) |

**Note:** Many vulnerabilities have been addressed through security tasks (SEC-027, SEC-038, SEC-045). See mitigation status in Risk Register.

### 5.3 Operational Vulnerabilities

| Vuln ID | Description | Affected Assets | Risk Level |
|---------|-------------|-----------------|------------|
| V-OP-001 | Private keys stored on developer workstation | DATA-001 | High |
| V-OP-002 | No formal key generation ceremony | DATA-001 | Medium |
| V-OP-003 | Insufficient access logging | All assets | Medium |
| V-OP-004 | Single person has key access | DATA-001 | High |

### 5.4 Supply Chain Vulnerabilities

| Vuln ID | Description | Affected Assets | Risk Level |
|---------|-------------|-----------------|------------|
| V-SUP-001 | Unverified third-party crypto libraries | SW-005 | Medium |
| V-SUP-002 | No hardware authenticity verification | HW-001 | Medium |
| V-SUP-003 | Build environment not reproducible | SW-006 | Medium |

---

## 6. Risk Analysis

### 6.1 Risk Identification Summary

| Risk ID | Threat | Vulnerability | Asset | Risk Score |
|---------|--------|-------------|-------|------------|
| R-001 | T-PHY-002 | V-HW-001 | HW-002 | 20 (Critical) |
| R-002 | T-FW-001 | V-HW-001 | SW-002 | 16 (High) |
| R-003 | T-FW-004 | V-OP-001 | DATA-001 | 25 (Critical) |
| R-004 | T-SUP-001 | - | HW-001 | 15 (High) |
| R-005 | T-NET-001 | V-SW-001 | SW-002 | 12 (High) |
| R-006 | T-SC-001 | V-SW-002 | DATA-005 | 9 (Medium) |
| R-007 | T-FW-003 | V-HW-001 | SW-002 | 12 (High) |
| R-008 | T-PHY-003 | V-HW-004 | DATA-001 | 10 (High) |
| R-009 | T-SUP-002 | V-SUP-002 | HW-001 | 8 (Medium) |
| R-010 | T-FW-002 | V-HW-002 | SW-001 | 15 (High) |

*Complete risk register available in `risk-register.md`*

### 6.2 Detailed Risk Analysis

#### R-001: Flash Extraction via Debug Interface

**Description:** An attacker with physical access can extract firmware using the SWD/JTAG interface if RDP (Read Protection) is not enabled.

| Factor | Assessment |
|--------|------------|
| Likelihood | Likely (4) - Physical access attacks are common |
| Impact | Major (5) - Complete firmware compromise |
| Risk Score | **20 (Critical)** |

**Current Controls:**
- RDP Level 1 available (blocks flash read)
- RDP Level 2 available (disables debug entirely)

**Gaps:**
- Production devices may ship with RDP Level 0
- No tamper detection if RDP bypass attempted

---

#### R-003: Signing Key Compromise

**Description:** Compromise of the Ed25519 private key allows an attacker to sign malicious firmware that would be accepted by devices.

| Factor | Assessment |
|--------|------------|
| Likelihood | Possible (3) - Key management is complex |
| Impact | Catastrophic (5) - Complete system compromise |
| Risk Score | **25 (Critical)** |

**Current Controls:**
- Keys generated offline
- Separate development/production keys

**Gaps:**
- No HSM usage for key storage
- Single-person key access
- No formal key ceremony documentation

---

## 7. Risk Treatment

### 7.1 Treatment Strategy

| Risk Level | Treatment Approach |
|------------|-------------------|
| Critical (17-25) | Immediate mitigation or avoidance required |
| High (10-16) | Mitigation required within 30 days |
| Medium (5-9) | Mitigation planned within 90 days |
| Low (1-4) | Accept with monitoring |

### 7.2 Treatment Options

#### Mitigation

Implementing controls to reduce likelihood or impact:

| Risk ID | Mitigation | Status |
|---------|------------|--------|
| R-001 | Enable RDP Level 2 in production | Implemented (SEC-045) |
| R-002 | Implement secure boot with signature verification | Implemented (SEC-045) |
| R-003 | Deploy HSM for key storage, implement key ceremony | Planned |
| R-005 | Input validation, buffer bounds checking | Implemented (SEC-038) |
| R-006 | Constant-time comparison functions | Implemented (SEC-027) |
| R-007 | Anti-rollback version checking | Implemented (SEC-045) |
| R-008 | Side-channel resistant crypto implementation | Partial |
| R-010 | Secure bootloader with signature verification | Implemented (SEC-045) |

#### Transfer

| Risk ID | Transfer Mechanism | Target |
|---------|-------------------|--------|
| R-004 | Component liability contract | STMicroelectronics |
| R-009 | Hardware authenticity verification | Supplier |

#### Acceptance

| Risk ID | Justification | Conditions |
|---------|-------------|------------|
| R-006 | Low likelihood with current mitigations | Monitor for new attacks |
| Low risks | Cost of mitigation exceeds impact | Annual review |

#### Avoidance

| Risk ID | Avoidance Strategy |
|---------|-------------------|
| - | Remove debug interfaces entirely (not applicable to development) |

### 7.3 Residual Risk

After treatment implementation:

| Risk Level | Before | After | Change |
|------------|--------|-------|--------|
| Critical | 4 | 1 | -3 |
| High | 9 | 3 | -6 |
| Medium | 11 | 8 | -3 |
| Low | 6 | 18 | +12 |
| **Total** | **30** | **30** | - |

**Residual Critical Risk:** R-003 (Key compromise) pending HSM implementation.

---

## 8. Risk Monitoring and Review

### 8.1 Monitoring Approach

| Activity | Frequency | Responsible |
|----------|-----------|-------------|
| Vulnerability scanning | Continuous (CI/CD) | Automated |
| Risk register review | Monthly | Security Lead |
| Threat intelligence review | Quarterly | Security Team |
| Full risk assessment | Annually | Security Officer |
| Post-incident review | After security events | Incident Response |

### 8.2 Risk Indicators

| Indicator | Threshold | Response |
|-----------|-----------|----------|
| New CVE in dependencies | Any | Assess impact within 24h |
| Failed security test | Any | Block release |
| Physical access incident | Any | Immediate assessment |
| Key access anomaly | Any | Immediate investigation |

### 8.3 Review Schedule

| Review Type | Frequency | Next Review |
|-------------|-----------|-------------|
| Risk register update | Monthly | 2026-05-18 |
| Threat model refresh | Quarterly | 2026-07-18 |
| Full assessment | Annual | 2027-04-18 |

---

## 9. Compliance Mapping

### 9.1 CISSP Domain 1: Security and Risk Management

| Control | Implementation |
|---------|----------------|
| Risk assessment methodology | Section 2.3 |
| Asset inventory | Section 3 |
| Risk treatment | Section 7 |

### 9.2 NIST CSF 2.0 GOVERN (GV.RM)

| Control | ID | Implementation |
|---------|-----|----------------|
| Risk management strategy | GV.RM-01 | Section 2 |
| Risk appetite statement | GV.RM-02 | Section 7.1 |
| Risk assessment process | GV.RM-03 | Section 6 |
| Risk response strategy | GV.RM-04 | Section 7 |
| Risk monitoring | GV.RM-05 | Section 8 |

### 9.3 ISO 27001:2022 A.5.36

| Control | Implementation |
|---------|----------------|
| A.5.36.1 | Sections 3, 4, 5, 6 |
| Risk acceptance criteria | Section 7.1 |

### 9.4 ISO 27001 Control Coverage

| Theme | Controls | Coverage |
|-------|----------|----------|
| Organizational (A.5) | 5.36 | ✓ Full |
| Technological (A.8) | 8.24, 8.25, 8.26 | ✓ Full |

---

## 10. Appendices

### Appendix A: Risk Assessment Process Flow

```
┌─────────────────┐
│ 1. Establish    │
│    Context      │
└────────┬────────┘
         ↓
┌─────────────────┐
│ 2. Identify     │
│    Assets       │
└────────┬────────┘
         ↓
┌─────────────────┐
│ 3. Identify     │
│    Threats      │
└────────┬────────┘
         ↓
┌─────────────────┐
│ 4. Identify     │
│    Vulnerabilities
└────────┬────────┘
         ↓
┌─────────────────┐
│ 5. Analyze      │
│    Risks        │
└────────┬────────┘
         ↓
┌─────────────────┐
│ 6. Evaluate     │
│    Risks        │
└────────┬────────┘
         ↓
┌─────────────────┐
│ 7. Treat        │
│    Risks        │
└────────┬────────┘
         ↓
┌─────────────────┐
│ 8. Monitor &    │
│    Review       │
└─────────────────┘
```

### Appendix B: Related Documents

| Document | Purpose |
|----------|---------|
| `risk-register.md` | Detailed risk register |
| `THREAT-MODEL.md` | System threat model |
| `SECURITY_TESTING.md` | Security testing procedures |
| `secure-boot.md` | Secure boot implementation |
| `safety.md` | Safety requirements |

### Appendix C: Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.0 | 2026-04-18 | Security Agent | Initial formal risk assessment |

### Appendix D: Approval

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Security Officer | | | |
| Project Manager | | | |
| Technical Lead | | | |

---

*End of Risk Assessment Document*
