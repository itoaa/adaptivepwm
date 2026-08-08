# AdaptivePWM Threat Model

## Overview

This document presents the threat model for AdaptivePWM using the STRIDE methodology, as recommended by IEC 62443 for industrial security.

**Version:** 1.1.0  
**Date:** 2026-04-18  
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
| UART/CLI | Unauthorized commands | High | CLI authentication (SEC-042) | ✅ Implemented |
| Debug interface | JTAG/SWD access | Medium | Physical security + RDP | ✅ Implemented |
| Bootloader | Malicious firmware | **Critical** | **Secure Boot (SEC-045)** | ✅ **IMPLEMENTED** |

### 2.2 Tampering (Integrity)

| Component | Threat | Risk | Mitigation | Status |
|-----------|--------|------|------------|--------|
| PWM signals | Duty cycle manipulation | Critical | Hardware limits (2-98%) | ✅ Implemented |
| Configuration | Parameter tampering | High | CRC validation | ✅ Implemented |
| Flash storage | Log tampering | Medium | Flash write protection | ✅ Implemented |
| Clock | Frequency tampering | High | CSS (Clock Security System) | ✅ Implemented |
| **Firmware** | **Malicious firmware flash** | **Critical** | **Ed25519 + RDP Level 2** | ✅ **IMPLEMENTED** |

### 2.3 Repudiation

| Component | Threat | Risk | Mitigation | Status |
|-----------|--------|------|------------|--------|
| Operations | Cannot prove what happened | Medium | Persistent flash logging | ✅ Implemented |
| Errors | Cannot trace fault source | Medium | Error log with timestamps | ✅ Implemented |

### 2.4 Information Disclosure

| Component | Threat | Risk | Mitigation | Status |
|-----------|--------|------|------------|--------|
| UART output | Sensitive data leakage | Low | No secrets in debug output | ✅ Implemented |
| Flash dump | Firmware extraction | Medium | Read protection enabled | ✅ Implemented |
| Power analysis | Side-channel attacks | Low | N/A (no crypto) | N/A |

### 2.5 Denial of Service

| Component | Threat | Risk | Mitigation | Status |
|-----------|--------|------|------------|--------|
| Watchdog | Intentional timeout trigger | High | Independent watchdog (500ms) | ✅ Implemented |
| Clock failure | HSE failure | High | CSS → HSI fallback | ✅ Implemented |
| PWM control | Malicious duty cycle commands | Critical | Rate limiting (±5%/10ms) | ✅ Implemented |
| Thermal | Thermal runaway | Critical | dT/dt monitoring + shutdown | ✅ Implemented |
| Electrical | Over-current/short-circuit | Critical | ADC monitoring + shutdown | ✅ Implemented |

### 2.6 Elevation of Privilege

| Component | Threat | Risk | Mitigation | Status |
|-----------|--------|------|------------|--------|
| CLI commands | Unauthorized config changes | Medium | Secure mode with auth (SEC-042) | ✅ Implemented |
| Debug mode | Production debug access | Medium | Debug disable in release | ✅ Implemented |

---

## 3. SECURE BOOT IMPLEMENTATION (SEC-045)

### 3.1 Security Finding Addressed

**Finding:** ADP-ARCH-001 (CVSS 8.1 - CRITICAL)  
**Description:** Secure Boot Not Implemented  
**Status:** ✅ **RESOLVED**

### 3.2 Implementation Summary

The secure boot feature addresses critical firmware tampering threats:

```
┌─────────────────────────────────────────────────────────────────┐
│  Bootloader (16KB) - RDP Level 2 Protected                      │
│  ├─ Ed25519 Signature Verification                              │
│  ├─ SHA-256 Hash Verification                                 │
│  ├─ Anti-rollback Version Check                                 │
│  └─ Recovery Mode (Authenticated Update)                       │
└─────────────────────────────────────────────────────────────────┘
                              ↓ (If Valid)
┌─────────────────────────────────────────────────────────────────┐
│  Signed Application Firmware (112KB max)                       │
│  ├─ 128-byte Header with Ed25519 Signature (64 bytes)         │
│  ├─ SHA-256 Hash of Firmware (32 bytes)                         │
│  └─ Monotonic Version Counter (Anti-rollback)                  │
└─────────────────────────────────────────────────────────────────┘
```

### 3.3 Verification Chain

1. **Magic Number** - Verify firmware format
2. **Size Check** - Ensure firmware fits in available flash
3. **Version Check** - Anti-rollback protection
4. **Hash Verification** - SHA-256 integrity check
5. **Signature Verification** - Ed25519 cryptographic proof

### 3.4 Flash Protection Levels

| Level | Protection | Debug | Production |
|-------|------------|-------|------------|
| 0 | None | Enabled | ❌ Development only |
| 1 | Read protection | Enabled | ❌ Not recommended |
| **2** | **Full protection** | **Disabled** | ✅ **Production** |

**⚠️ RDP Level 2 is IRREVERSIBLE**

---

## 4. RISK ASSESSMENT

### 4.1 Risk Matrix

| Threat | Likelihood (1-5) | Impact (1-5) | Risk Score | Priority | Status |
|--------|------------------|--------------|------------|----------|--------|
| **Firmware tampering** | 2 | 5 | **10** | **High** | ✅ **RESOLVED** |
| PWM manipulation | 2 | 5 | 10 | High | ✅ Implemented |
| Clock failure | 2 | 4 | 8 | High | ✅ Implemented |
| Thermal runaway | 2 | 5 | 10 | High | ✅ Implemented |
| Over-current | 3 | 5 | 15 | Critical | ✅ Implemented |
| Configuration tampering | 2 | 3 | 6 | Medium | ✅ Implemented |
| Information disclosure | 2 | 2 | 4 | Low | Accepted |

### 4.2 Post-Implementation Risk Reduction

**Critical Finding ADP-ARCH-001 (Secure Boot):**
- **Before:** CVSS 8.1 (High) - Firmware could be replaced via debug interface
- **After:** CVSS 2.3 (Low) - Requires physical access + private key
- **Risk Reduction:** 72% improvement

---

## 5. ATTACK SCENARIOS

### Scenario 1: Malicious Firmware Flash (ADDRESSED)

**Attacker:** Physical access to device  
**Goal:** Install malicious firmware  
**Before Mitigation:**
1. Connect via JTAG/SWD
2. Flash malicious firmware
3. Device boots malicious code

**After Mitigation (SEC-045):**
1. Connect via JTAG/SWD → **RDP Level 2 blocks access**
2. Attempt firmware update → **Requires Ed25519 signature**
3. Without private key → **Bootloader rejects firmware**
4. Device enters recovery mode → **Requires authentication**

**Residual Risk:** Attacker with physical access AND private key

### Scenario 2: Rollback Attack (ADDRESSED)

**Attacker:** Physical access  
**Goal:** Rollback to vulnerable firmware version  
**Mitigation:** Monotonic version counter in flash  
- New version must be ≥ stored version
- Prevents downgrade to vulnerable firmware

### Scenario 3: Malicious PWM Commands

**Attacker:** External system via UART  
**Goal:** Cause hardware damage through excessive duty cycle  
**Steps:**
1. Connect to UART
2. Send PWM duty = 100%

**Mitigation:**
- SR-003: Hardware limits reject 100%
- SR-006: Rate limiting prevents rapid changes

### Scenario 4: Clock Attack

**Attacker:** EMI or hardware tampering  
**Goal:** Disrupt timing to cause unpredictable behavior  
**Steps:**
1. Inject EMI to disturb HSE crystal

**Mitigation:**
- SR-001: CSS detects failure, switches to HSI
- System continues at reduced performance

### Scenario 5: Thermal Runaway

**Attacker:** N/A (system failure)  
**Risk:** Component overheating and fire  
**Steps:**
1. Cooling system failure
2. Temperature rises rapidly

**Mitigation:**
- SR-007: dT/dt monitoring triggers shutdown
- SR-002: Watchdog ensures responsiveness

---

## 6. SECURITY REQUIREMENTS MAPPING

| Threat Category | Security Requirement | Implementation | Status |
|----------------|---------------------|----------------|--------|
| **Tampering (Firmware)** | **SEC-045: Secure Boot** | **Ed25519 + RDP Level 2** | ✅ **RESOLVED** |
| Tampering (PWM) | SR-003: Boundary validation | Hardware limits 2-98% | ✅ Implemented |
| Tampering (Clock) | SR-001: Clock Security System | CSS enabled | ✅ Implemented |
| DoS (Watchdog) | SR-002: Watchdog Implementation | 500ms timeout, 100ms refresh | ✅ Implemented |
| DoS (PWM) | SR-006: Rate-of-change limiting | ±5%/10ms | ✅ Implemented |
| DoS (Thermal) | SR-007: Thermal runaway protection | dT/dt > 5°C/s → shutdown | ✅ Implemented |
| DoS (Electrical) | SR-008: Over-current detection | 110%/150% thresholds | ✅ Implemented |
| Spoofing (CLI) | SEC-042: CLI Authentication | HMAC-SHA256 | ✅ Implemented |
| Information disclosure | N/A (accepted risk) | No sensitive data | N/A |

---

## 7. RECOMMENDED ADDITIONAL MEASURES

### 7.1 Future Enhancements

| Priority | Measure | Target | Status |
|----------|---------|--------|--------|
| ~~High~~ | ~~Secure boot~~ | ~~v3.0~~ | ✅ **DONE** (SEC-045) |
| ~~High~~ | ~~CLI authentication~~ | ~~v2.2~~ | ✅ **DONE** (SEC-042) |
| ~~Medium~~ | ~~Firmware signing~~ | ~~v3.0~~ | ✅ **DONE** (SEC-045) |
| ~~Medium~~ | ~~Debug interface disable~~ | ~~v2.2~~ | ✅ **DONE** |
| Medium | HSM for key storage | Future | Planned |
| Low | Side-channel countermeasures | Future | Planned |

### 7.2 Key Management Best Practices

1. **Store private keys securely**
   - Use HSM (YubiHSM, AWS CloudHSM) for production
   - Offline storage for backup
   - Never commit to version control

2. **Key rotation procedure**
   - Document key generation ceremony
   - Multiple witnesses
   - Secure transport to HSM

3. **Separation of concerns**
   - Separate keys for dev/prod
   - Separate keys per product line
   - Emergency revocation plan

---

## 8. REVIEW PROCESS

This threat model MUST be reviewed:
- After any architecture change
- After any security incident
- Quarterly minimum
- After implementing security features

---

**Document Control**

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.0 | 2026-03-22 | Assistant | Initial threat model |
| 1.1.0 | 2026-04-18 | coding-agent | Added SEC-045 Secure Boot, updated all statuses |

**Next Review:** 2026-07-18

---

## Appendix A: References

- **Secure Boot Documentation:** `docs/secure-boot.md`
- **Security Assessment Report:** `reports/security-assessment-2026-04-18.md`
- **Task SEC-045:** `tasks/SEC-045.md`
- **Task SEC-042:** `tasks/SEC-042.md`
- **Ed25519 RFC:** RFC 8032
- **STM32 RDP:** STM32F4 Reference Manual
- **IEC 62443:** Industrial security standard
