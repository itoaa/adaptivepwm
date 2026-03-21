# Security Review Template

**AdaptivePWM - CISSP-Aligned Security Review**

**Version:** 1.0  
**Classification:** Internal  
**Required for:** Critical/Major changes, Security features

---

## Review Information

| Field | Value |
|-------|-------|
| Review ID | SEC-YYYY-MM-DD-XXX |
| Change Request | [Link to CR] |
| Reviewer | Name |
| Date | YYYY-MM-DD |
| Status | [In Progress / Approved / Rejected / Needs Revision] |

### Change Summary

```
Brief description of the change and its security implications.
```

---

## CISSP Domain Analysis

### Domain 3: Security Architecture and Engineering

#### Clock System Integrity
- [ ] CSS (Clock Security System) is enabled if HSE is used
- [ ] Watchdog is properly configured (≤500ms timeout)
- [ ] Clock failure detection implemented
- [ ] Fail-safe clock switching verified

**Assessment:**
```
Comments on clock system security
```

#### Defense in Depth
- [ ] Multiple security layers implemented
- [ ] No single point of failure
- [ ] Security boundaries identified and protected
- [ ] Hardware protection features utilized

**Assessment:**
```
```

### Domain 4: Communication and Network Security

#### UART/CLI Security
- [ ] Input validation implemented
- [ ] Buffer overflow prevention
- [ ] Authentication required for critical commands
- [ ] Command injection prevention

**Assessment:**
```
```

#### Data Integrity
- [ ] CRC/checksums for critical data
- [ ] Flash write verification
- [ ] Configuration validation on load

**Assessment:**
```
```

### Domain 5: Identity and Access Management

#### Access Control
- [ ] Secure mode properly implemented
- [ ] Privilege separation
- [ ] Certificate validation (if applicable)
- [ ] Authentication tokens handled securely

**Assessment:**
```
```

### Domain 6: Security Assessment and Testing

#### Static Analysis
- [ ] No hardcoded secrets
- [ ] No buffer overflow vulnerabilities
- [ ] Integer overflow checked
- [ ] Null pointer dereferences prevented

**Tools Used:**
- [ ] PC-lint Plus
- [ ] Cppcheck
- [ ] Manual review

**Assessment:**
```
```

#### Dynamic Analysis
- [ ] Fuzz testing performed
- [ ] Fault injection tested
- [ ] Boundary conditions tested
- [ ] Error paths exercised

**Assessment:**
```
```

### Domain 8: Software Development Security

#### Secure Coding Practices
- [ ] Input validation on all entry points
- [ ] Output encoding where needed
- [ ] Resource cleanup on all paths
- [ ] Principle of least privilege applied

**Assessment:**
```
```

#### Documentation
- [ ] Security considerations documented
- [ ] API documentation updated
- [ ] Threat model updated (if needed)
- [ ] Security test cases written

**Assessment:**
```
```

---

## Threat Analysis

### STRIDE Analysis

| Threat | Category | Risk | Mitigation | Status |
|--------|----------|------|------------|--------|
| Spoofing | Authentication | | | |
| Tampering | Integrity | | | |
| Repudiation | Non-repudiation | | | |
| Information Disclosure | Confidentiality | | | |
| Denial of Service | Availability | | | |
| Elevation of Privilege | Authorization | | | |

### Attack Surface Analysis

**New Attack Surfaces Introduced:**
```
List any new interfaces, inputs, or functionality that could be exploited
```

**Attack Surface Reduction:**
```
Describe any attack surfaces that have been reduced or eliminated
```

---

## Risk Assessment

### Risk Calculation

| Factor | Score (1-5) | Notes |
|--------|-------------|-------|
| Likelihood | | |
| Impact | | |
| **Total Risk** | **L × I** | |

### Risk Matrix

```
     Impact
       1    2    3    4    5
    ┌────┬────┬────┬────┬────┐
  5 │  5 │ 10 │ 15 │ 20 │ 25 │
  4 │  4 │  8 │ 12 │ 16 │ 20 │
L 3 │  3 │  6 │  9 │ 12 │ 15 │
I 2 │  2 │  4 │  6 │  8 │ 10 │
K 1 │  1 │  2 │  3 │  4 │  5 │
    └────┴────┴────┴────┴────┘
```

**Risk Level:**
- 1-5: Low (Acceptable)
- 6-12: Medium (Mitigation recommended)
- 15-25: High (Mitigation required)

### Mitigations Implemented

| Risk | Mitigation | Verification |
|------|------------|--------------|
| | | |

---

## Security Testing

### Test Cases

| Test ID | Description | Expected Result | Status |
|---------|-------------|-----------------|--------|
| SEC-001 | Clock failure detection | CSS triggers | |
| SEC-002 | Watchdog timeout | System reset | |
| SEC-003 | Buffer overflow | Crash prevented | |
| SEC-004 | Input validation | Invalid input rejected | |
| SEC-005 | Authentication bypass | Access denied | |

### Penetration Testing

**Scope:**
```
What was tested and how
```

**Findings:**
```
Any vulnerabilities discovered
```

**Remediation:**
```
How findings were addressed
```

---

## Compliance Check

### Standards Compliance

| Standard | Requirement | Status | Evidence |
|----------|-------------|--------|----------|
| CISSP Framework | Security by Design | | |
| MISRA-C:2012 | Coding standards | | |
| IEC 61508 | Functional safety | | |

### Security Requirements Verification

| Req ID | Description | Status | Notes |
|--------|-------------|--------|-------|
| SR-001 | CSS Enabled | | |
| SR-002 | Watchdog Configured | | |
| SR-003 | Boundary Validation | | |
| SR-004 | Fail-Safe Defaults | | |
| SR-005 | Secure Boot (Future) | N/A | |

---

## Review Decision

### Approvals

**Security Officer:**
- [ ] Approved
- [ ] Approved with conditions
- [ ] Rejected
- [ ] Needs revision

**Conditions (if any):**
```
List any conditions that must be met
```

**Technical Lead:**
- [ ] Approved
- [ ] Approved with conditions
- [ ] Rejected

**Project Owner:**
- [ ] Approved
- [ ] Approved with conditions
- [ ] Rejected

### Signatures

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Security Officer | | | |
| Technical Lead | | | |
| Project Owner | | | |

---

## Post-Review Actions

### Action Items

| ID | Action | Owner | Due Date | Status |
|----|--------|-------|----------|--------|
| | | | | |

### Follow-up Review Required
- [ ] Yes
- [ ] No

**If yes, conditions for re-review:**
```
```

---

## Appendix

### Security Testing Results

[Attach test logs, coverage reports, static analysis reports]

### References

- [Project Framework](../../PROJECT_FRAMEWORK.md)
- [Security Documentation](../safety.md)
- [Threat Model] (if exists)
- [Previous Security Reviews]

---

**Template Version:** 1.0  
**Last Updated:** 2026-03-21
