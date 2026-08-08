# AdaptivePWM Risk Register

**Document ID:** ADP-RISK-001-REG  
**Version:** 1.0.0  
**Date:** 2026-04-18  
**Classification:** Security Documentation

---

## Overview

This risk register contains all identified risks for the AdaptivePWM project. It is a living document that should be reviewed and updated monthly.

**Register Statistics:**

| Status | Count |
|--------|-------|
| Total Risks | 30 |
| Critical | 4 |
| High | 9 |
| Medium | 11 |
| Low | 6 |
| Open | 8 |
| Mitigated | 18 |
| Accepted | 4 |

---

## Risk Register Table

### Critical Risks (Score 17-25)

| ID | Category | Threat | Asset | Likelihood | Impact | Score | Status | Treatment | Owner | Due Date |
|----|----------|--------|-------|------------|--------|-------|--------|-----------|-------|----------|
| R-001 | Hardware | Debug interface exploitation | Firmware (HW-002) | 4 | 5 | **20** | Mitigated | RDP Level 2 + Secure Boot | Security | 2026-04-18 |
| R-003 | Operational | Signing key compromise | Private Keys (DATA-001) | 3 | 5 | **15→25** | Open | HSM + Key Ceremony | Security | 2026-05-30 |
| R-020 | Supply Chain | Compromised MCU chips | STM32F401RE (HW-001) | 3 | 5 | **15** | Open | Hardware authenticity verification | Procurement | 2026-06-30 |
| R-021 | Software | Malicious code injection | Source Code (SW-007) | 2 | 5 | **10** | Mitigated | Code review + CI/CD security | Security | 2026-04-18 |

*Note: R-003 score increased from 15 to 25 upon detailed analysis of key compromise impact.*

### High Risks (Score 10-16)

| ID | Category | Threat | Asset | Likelihood | Impact | Score | Status | Treatment | Owner | Due Date |
|----|----------|--------|-------|------------|--------|-------|--------|-----------|-------|----------|
| R-002 | Hardware | Unauthorized firmware flashing | Firmware (SW-002) | 4 | 4 | **16** | Mitigated | Secure boot (SEC-045) | Security | 2026-04-18 |
| R-004 | Supply Chain | Compromised dev tools | Build System (SW-006) | 3 | 5 | **15** | Open | Toolchain verification | Security | 2026-06-15 |
| R-005 | Network | UART command injection | Firmware (SW-002) | 4 | 3 | **12** | Mitigated | Input validation (SEC-038) | Security | 2026-04-18 |
| R-007 | Firmware | Firmware rollback attack | Firmware (SW-002) | 4 | 3 | **12** | Mitigated | Anti-rollback (SEC-045) | Security | 2026-04-18 |
| R-008 | Side-Channel | Power analysis attack | Keys (DATA-001) | 4 | 3 | **12** | Partial | Side-channel resistant crypto | Security | 2026-06-30 |
| R-010 | Firmware | Bootloader bypass | Bootloader (SW-001) | 3 | 5 | **15** | Mitigated | Ed25519 verification (SEC-045) | Security | 2026-04-18 |
| R-011 | Software | Buffer overflow in CLI | CLI (SW-009) | 3 | 4 | **12** | Mitigated | Bounds checking (SEC-038) | Security | 2026-04-18 |
| R-012 | Software | Integer overflow | Bootloader (SW-001) | 3 | 4 | **12** | Mitigated | Safe arithmetic | Security | 2026-04-18 |
| R-015 | Operational | Insider key exfiltration | Keys (DATA-001) | 3 | 4 | **12** | Open | Key splitting + audit | Security | 2026-05-30 |

### Medium Risks (Score 5-9)

| ID | Category | Threat | Asset | Likelihood | Impact | Score | Status | Treatment | Owner | Due Date |
|----|----------|--------|-------|------------|--------|-------|--------|-----------|-------|----------|
| R-006 | Side-Channel | Timing analysis on auth | Passwords (DATA-005) | 3 | 3 | **9** | Mitigated | Constant-time compare (SEC-027) | Security | 2026-04-18 |
| R-009 | Supply Chain | Unverified crypto libs | Crypto (SW-005) | 3 | 3 | **9** | Open | Library audit + verification | Security | 2026-05-15 |
| R-013 | Hardware | No tamper detection | MCU (HW-001) | 2 | 4 | **8** | Accepted | Monitor for attacks | Security | Ongoing |
| R-014 | Software | Insufficient PBKDF2 iter | Passwords (DATA-005) | 3 | 3 | **9** | Mitigated | 100k iterations (SEC-027) | Security | 2026-04-18 |
| R-016 | Operational | Inadequate access logging | All assets | 3 | 3 | **9** | Open | Centralized logging | Security | 2026-06-30 |
| R-017 | Operational | Single-person key access | Keys (DATA-001) | 2 | 4 | **8** | Open | Multi-person rule | Security | 2026-05-15 |
| R-018 | Network | Firmware update interception | Firmware (SW-002) | 2 | 4 | **8** | Mitigated | Signed updates (SEC-045) | Security | 2026-04-18 |
| R-019 | Physical | Cold boot attack | RAM (HW-003) | 2 | 3 | **6** | Accepted | Limited exposure time | Security | Ongoing |
| R-022 | Firmware | Recovery mode exploitation | Bootloader (SW-001) | 2 | 4 | **8** | Mitigated | Auth'd recovery only | Security | 2026-04-18 |
| R-023 | Network | Replay attacks | UART (HW-006) | 2 | 3 | **6** | Accepted | Physical access required | Security | Ongoing |
| R-024 | Software | Config injection | Config (DATA-002) | 2 | 3 | **6** | Mitigated | Input validation | Security | 2026-04-18 |

### Low Risks (Score 1-4)

| ID | Category | Threat | Asset | Likelihood | Impact | Score | Status | Treatment | Owner | Due Date |
|----|----------|--------|-------|------------|--------|-------|--------|-----------|-------|----------|
| R-025 | Physical | Device theft | Hardware | 2 | 2 | **4** | Accepted | Physical security policy | Ops | Ongoing |
| R-026 | Operational | Lost test equipment | Test Equip (HW-010) | 1 | 2 | **2** | Accepted | Asset tracking | Ops | Ongoing |
| R-027 | Software | Log tampering | Logs (DATA-003) | 1 | 3 | **3** | Accepted | HMAC protection | Security | Ongoing |
| R-028 | Network | Protocol fuzzing | UART (HW-006) | 2 | 2 | **4** | Accepted | Fuzz testing in CI | Security | Ongoing |
| R-029 | Side-Channel | EM analysis | MCU (HW-001) | 1 | 3 | **3** | Accepted | Limited threat model | Security | Ongoing |
| R-030 | Supply Chain | Documentation leak | Docs (DOC-003) | 1 | 2 | **2** | Accepted | Access controls | Security | Ongoing |

---

## Risk Details

### R-001: Debug Interface Exploitation

**Description:**
Physical attackers can use SWD/JTAG to extract firmware if RDP is not properly configured.

**Current Status:** Mitigated

**Mitigation Evidence:**
- [x] RDP Level 1 enabled in development
- [x] RDP Level 2 enforced in production (SEC-045)
- [x] Secure boot prevents unsigned firmware execution
- [ ] Tamper detection (not implemented)

**Residual Risk:** Low - Physical attack with specialized equipment still theoretically possible but extremely difficult with RDP Level 2.

---

### R-003: Signing Key Compromise

**Description:**
Compromise of Ed25519 private keys allows attackers to sign malicious firmware that devices will accept.

**Current Status:** Open (Critical)

**Required Mitigations:**
- [ ] Deploy HSM (YubiHSM or AWS KMS) for production keys
- [ ] Document formal key generation ceremony
- [ ] Implement key splitting (M-of-N)
- [ ] Regular key rotation schedule
- [ ] Air-gapped key generation workstation

**Timeline:**
- HSM procurement: 2026-05-15
- Key ceremony: 2026-05-30
- Production deployment: 2026-06-15

**Business Impact:** Complete system compromise if exploited.

---

### R-002: Unauthorized Firmware Flashing

**Description:**
Attackers with physical access can attempt to flash modified firmware directly to device flash.

**Current Status:** Mitigated

**Evidence:**
- Secure bootloader verifies Ed25519 signatures before execution
- Firmware header includes version (anti-rollback)
- SHA-256 hash verified before signature check

**Verification:** Security tests in `tests/security/test_secure_boot.c`

---

### R-005: UART Command Injection

**Description:**
Malicious input via CLI could trigger buffer overflows or command injection.

**Current Status:** Mitigated

**Evidence:**
- Input length limits enforced
- Command whitelist implemented
- Buffer overflow tests passing (SEC-038)
- Cppcheck security scans clean

---

### R-007: Firmware Rollback Attack

**Description:**
Attackers may attempt to install older firmware versions with known vulnerabilities.

**Current Status:** Mitigated

**Evidence:**
- Monotonic version counter in firmware header
- Secure boot verifies version >= stored version
- Version stored in tamper-resistant flash area

---

### R-008: Power Analysis Attack

**Description:**
Advanced attackers can extract cryptographic keys by analyzing power consumption patterns.

**Current Status:** Partial

**Current Protections:**
- Ed25519 from TweetNaCl (minimal timing variability)
- No dedicated side-channel countermeasures

**Planned Mitigations:**
- Power analysis resistant crypto library evaluation
- Hardware masking techniques research
- White-box crypto consideration

**Risk Acceptance:** Current threat model assumes attackers with this capability are outside scope for this product.

---

### R-020: Compromised MCU Chips

**Description:**
Supply chain insertion of malicious or backdoored microcontroller chips.

**Current Status:** Open

**Mitigation Strategy:**
- [ ] Establish authorized distributor relationships
- [ ] Implement chip authenticity verification
- [ ] Consider secure element addition
- [ ] Firmware attestation checks

---

## Risk Trends

### Risk Score Over Time

```
Score
25 ┤                    ●
20 ┤    ●
15 ┤         ●    ●              ●
10 ┤              ●    ●    ●
 5 ┤ ┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┐
   │ Jan  Feb  Mar  Apr  May  Jun  Jul  Aug │
   └────────────────────────────────────────┘
```

### Treatment Progress

| Month | Open | Mitigated | Accepted | Total |
|-------|------|-----------|----------|-------|
| Mar 2026 | 25 | 3 | 2 | 30 |
| Apr 2026 | 8 | 18 | 4 | 30 |
| May 2026 (proj) | 5 | 21 | 4 | 30 |

---

## Action Items

### Open Actions

| ID | Action | Risk | Priority | Owner | Due |
|----|--------|------|----------|-------|-----|
| A-001 | Deploy HSM for key storage | R-003 | Critical | Security | 2026-05-30 |
| A-002 | Document key ceremony | R-003 | Critical | Security | 2026-05-30 |
| A-003 | Implement key splitting | R-015 | High | Security | 2026-05-30 |
| A-004 | Toolchain verification | R-004 | High | Security | 2026-06-15 |
| A-005 | Crypto library audit | R-009 | Medium | Security | 2026-05-15 |
| A-006 | Centralized logging | R-016 | Medium | Security | 2026-06-30 |
| A-007 | Multi-person key rule | R-017 | Medium | Security | 2026-05-15 |
| A-008 | Hardware authenticity | R-020 | High | Procurement | 2026-06-30 |

---

## Review History

| Date | Reviewer | Changes |
|------|----------|---------|
| 2026-04-18 | Security Agent | Initial risk register created |

---

## Next Review

**Scheduled:** 2026-05-18

**Required Attendees:**
- Security Officer
- Project Manager
- Technical Lead
- Operations Representative

---

*End of Risk Register*
