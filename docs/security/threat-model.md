# AdaptivePWM Threat Model

## Overview

This document presents the threat model for AdaptivePWM using the STRIDE methodology, as recommended by IEC 62443 for industrial security.

**Version:** 1.0.0  
**Date:** 2026-03-22  
**Classification:** Internal  
**Methodology:** STRIDE (Spoofing, Tampering, Repudiation, Information Disclosure, Denial of Service, Elevation of Privilege)

---

## 1. SYSTEM CONTEXT

### 1.1 Data Flow Diagram (DFD)

```
┌──────────────┐      ┌──────────────┐      ┌──────────────┐
│   External   │      │  Adaptive   │      │   External   │
│   Control    │◄────►│    PWM      │◄────►│    Load      │
│   System     │ UART │   System    │ PWM  │  (Motor/     │
│              │      │             │      │  Converter)  │
└──────────────┘      └──────┬──────┘      └──────────────┘
                             │
                             │ ADC
                             ▼
                       ┌──────────────┐
                       │   Sensors    │
                       │ (V, I, T)    │
                       └──────────────┘
```

### 1.2 Trust Boundaries

| Boundary | Description | Trust Level |
|----------|-------------|-------------|
| B1 | External Control ↔ PWM System | Untrusted |
| B2 | PWM System ↔ External Load | Semi-trusted |
| B3 | PWM System ↔ Sensors | Trusted |
| B4 | Internal components | Trusted |

---

## 2. STRIDE ANALYSIS

### 2.1 Spoofing (Authentication)

| Component | Threat | Risk | Mitigation | Status |
|-----------|--------|------|------------|--------|
| UART/CLI | Unauthorized commands | High | CLI authentication (future) | Planned |
| Debug interface | JTAG/SWD access | Medium | Physical security | Required |
| Bootloader | Malicious firmware | High | Secure boot (v3.0) | Planned |

### 2.2 Tampering (Integrity)

| Component | Threat | Risk | Mitigation | Status |
|-----------|--------|------|------------|--------|
| PWM signals | Duty cycle manipulation | Critical | Hardware limits (2-98%) | Implemented |
| Configuration | Parameter tampering | High | CRC validation | Implemented |
| Flash storage | Log tampering | Medium | Flash write protection | Implemented |
| Clock | Frequency tampering | High | CSS (Clock Security System) | Implemented |

### 2.3 Repudiation

| Component | Threat | Risk | Mitigation | Status |
|-----------|--------|------|------------|--------|
| Operations | Cannot prove what happened | Medium | Persistent flash logging | Implemented |
| Errors | Cannot trace fault source | Medium | Error log with timestamps | Implemented |

### 2.4 Information Disclosure

| Component | Threat | Risk | Mitigation | Status |
|-----------|--------|------|------------|--------|
| UART output | Sensitive data leakage | Low | No secrets in debug output | Implemented |
| Flash dump | Firmware extraction | Medium | Read protection enabled | Required |
| Power analysis | Side-channel attacks | Low | N/A (no crypto) | N/A |

### 2.5 Denial of Service

| Component | Threat | Risk | Mitigation | Status |
|-----------|--------|------|------------|--------|
| Watchdog | Intentional timeout trigger | High | Independent watchdog (500ms) | Implemented |
| Clock failure | HSE failure | High | CSS → HSI fallback | Implemented |
| PWM control | Malicious duty cycle commands | Critical | Rate limiting (±5%/10ms) | Implemented |
| Thermal | Thermal runaway | Critical | dT/dt monitoring + shutdown | Implemented |
| Electrical | Over-current/short-circuit | Critical | ADC monitoring + shutdown | Implemented |

### 2.6 Elevation of Privilege

| Component | Threat | Risk | Mitigation | Status |
|-----------|--------|------|------------|--------|
| CLI commands | Unauthorized config changes | Medium | Secure mode with auth | Planned |
| Debug mode | Production debug access | Medium | Debug disable in release | Required |

---

## 3. RISK ASSESSMENT

### 3.1 Risk Matrix

| Threat | Likelihood (1-5) | Impact (1-5) | Risk Score | Priority |
|--------|------------------|--------------|------------|----------|
| PWM manipulation | 2 | 5 | 10 | High |
| Clock failure | 2 | 4 | 8 | High |
| Thermal runaway | 2 | 5 | 10 | High |
| Over-current | 3 | 5 | 15 | Critical |
| Configuration tampering | 2 | 3 | 6 | Medium |
| Information disclosure | 2 | 2 | 4 | Low |

### 3.2 Mitigation Priorities

**Critical (Score ≥ 15):**
- SR-008: Over-current/short-circuit detection

**High (Score 8-14):**
- SR-001: Clock Security System
- SR-006: PWM rate-of-change limiting
- SR-007: Thermal runaway protection

**Medium (Score 4-7):**
- SR-003: Boundary validation
- Configuration integrity

**Low (Score < 4):**
- Information disclosure (accepted risk)

---

## 4. ATTACK SCENARIOS

### Scenario 1: Malicious PWM Commands

**Attacker:** External system via UART  
**Goal:** Cause hardware damage through excessive duty cycle  
**Steps:**
1. Connect to UART
2. Send PWM duty = 100%

**Mitigation:**
- SR-003: Hardware limits reject 100%
- SR-006: Rate limiting prevents rapid changes

### Scenario 2: Clock Attack

**Attacker:** EMI or hardware tampering  
**Goal:** Disrupt timing to cause unpredictable behavior  
**Steps:**
1. Inject EMI to disturb HSE crystal

**Mitigation:**
- SR-001: CSS detects failure, switches to HSI
- System continues at reduced performance

### Scenario 3: Thermal Runaway

**Attacker:** N/A (system failure)  
**Risk:** Component overheating and fire  
**Steps:**
1. Cooling system failure
2. Temperature rises rapidly

**Mitigation:**
- SR-007: dT/dt monitoring triggers shutdown
- SR-002: Watchdog ensures responsiveness

---

## 5. SECURITY REQUIREMENTS MAPPING

| Threat Category | Security Requirement | Implementation |
|----------------|---------------------|----------------|
| Tampering (PWM) | SR-003: Boundary validation | Hardware limits 2-98% |
| Tampering (Clock) | SR-001: Clock Security System | CSS enabled |
| DoS (Watchdog) | SR-002: Watchdog Implementation | 500ms timeout, 100ms refresh |
| DoS (PWM) | SR-006: Rate-of-change limiting | ±5%/10ms |
| DoS (Thermal) | SR-007: Thermal runaway protection | dT/dt > 5°C/s → shutdown |
| DoS (Electrical) | SR-008: Over-current detection | 110%/150% thresholds |
| Information disclosure | N/A (accepted risk) | No sensitive data |

---

## 6. RECOMMENDED ADDITIONAL MEASURES

### 6.1 Future Enhancements

| Priority | Measure | Target Version |
|----------|---------|----------------|
| High | Secure boot (SR-005) | v3.0 |
| High | CLI authentication | v2.2 |
| Medium | Firmware signing | v3.0 |
| Medium | Debug interface disable | v2.2 |
| Low | Power analysis countermeasures | Future |

### 6.2 Physical Security

- JTAG/SWD access requires physical presence
- Debug interface can be disabled via option bytes
- Production firmware should disable debug

---

## 7. REVIEW PROCESS

This threat model MUST be reviewed:
- After any architecture change
- After any security incident
- Quarterly minimum

---

**Document Control**

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.0 | 2026-03-22 | Assistant | Initial threat model with STRIDE analysis |

**Next Review:** 2026-06-22
