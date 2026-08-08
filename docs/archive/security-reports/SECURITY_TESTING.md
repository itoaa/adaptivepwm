# AdaptivePWM Security Testing Guide

**Document ID:** SEC-038  
**Version:** 1.0.0  
**Date:** 2026-04-16  
**Classification:** Technical Documentation  
**Framework:** CISSP Domain 6, ISO 27001:2022 8.25, NIST CSF DE.AE

---

## 1. Overview

This document describes the automated security testing framework implemented for AdaptivePWM as part of SEC-038. The security testing pipeline integrates static analysis tools and security-focused test cases to identify vulnerabilities early in the development lifecycle.

### 1.1 Security Framework Alignment

| Framework | Control/Domain | Description |
|-----------|--------------|-------------|
| CISSP | Domain 6 | Security Assessment and Testing |
| CISSP | Domain 8 | Software Development Security |
| ISO 27001:2022 | 8.25 | Secure development lifecycle |
| ISO 27001:2022 | 8.26 | Application security requirements |
| NIST CSF 2.0 | DE.AE | Anomalies and Events detection |
| NIST CSF 2.0 | PR.DS | Data Security protection |

---

## 2. Security Testing Tools

### 2.1 Cppcheck - Static Analysis

**Configuration:** `ci/cppcheck-security.cfg`  
**Suppressions:** `ci/cppcheck-suppressions.txt`

Cppcheck is configured with security-focused addons:
- `security` addon: Detects security-related issues
- `cert` addon: Checks for CERT C Secure Coding Standard violations

#### Enabled Checks

| Check Type | Description |
|------------|-------------|
| `bufferOverflow` | Buffer overflow detection |
| `memoryLeak` | Memory leak detection |
| `nullPointer` | Null pointer dereference |
| `dangerousFunctions` | Dangerous function usage (strcpy, sprintf, etc.) |
| `integerOverflow` | Integer overflow detection |
| `uninitialized` | Uninitialized variable usage |
| `security` | All security-specific checks |

#### Running Cppcheck

```bash
# Using make
make security-cppcheck

# Manual command
cppcheck \
    --enable=all \
    --addon=security \
    --addon=cert \
    --suppressions-list=ci/cppcheck-suppressions.txt \
    --template='{file}:{line}:{column}: {severity}: {message} [{id}]' \
    src/ include/
```

### 2.2 Clang-Tidy - Security Analysis

**Configuration:** `.clang-tidy-security`

Clang-Tidy is configured with security-focused checks based on:
- **CERT C Secure Coding Standard**
- **C++ Core Guidelines** (security-related)
- **Bug-prone patterns** that lead to security issues

#### Enabled Security Check Categories

| Category | Description |
|----------|-------------|
| `security-*` | All security checks |
| `cert-*` | CERT C Secure Coding Standard |
| `cppcoreguidelines-*` | C++ Core Guidelines (security-related) |
| `bugprone-*` | Bug-prone patterns |
| `performance-*` | Memory safety performance checks |

#### Running Clang-Tidy

```bash
# Using make
make security-clang-tidy

# Manual command
find src -name "*.c" | while read file; do
    clang-tidy "$file" --config-file=.clang-tidy-security -- -Isrc -Iinclude
done
```

---

## 3. Security Test Suite

### 3.1 Test File Structure

```
tests/security/
├── test_auth_overflow.c       # CWE-120, CWE-121, CWE-122
├── test_auth_timing.c         # CWE-208, CWE-203
├── test_pbkdf2_iterations.c   # CWE-916, NIST SP 800-132
├── test_hmac_verification.c   # CWE-353, CWE-354
├── test_cli_input.c           # CWE-20, CWE-78, CWE-88
└── test_config_parsing.c      # CWE-20, CWE-676, CWE-119
```

### 3.2 Security Test Categories

#### Authentication Security (`test_auth_overflow.c`)

| Test | CWE | Description |
|------|-----|-------------|
| Buffer overflow protection | CWE-120, CWE-121, CWE-122 | Tests buffer bounds checking |
| Null byte injection | CWE-158 | Tests null byte handling in passwords |
| Format string protection | CWE-134 | Tests printf format string safety |
| Integer overflow | CWE-190 | Tests length calculation overflow |
| Off-by-one errors | CWE-193 | Tests buffer boundary handling |

#### Timing Attack Resistance (`test_auth_timing.c`)

| Test | CWE | Description |
|------|-----|-------------|
| Constant-time comparison | CWE-208 | Tests timing-safe comparison |
| Timing consistency | CWE-208 | Tests execution time uniformity |
| Vulnerable comparison | CWE-203 | Demonstrates timing leaks |
| Password verification | CWE-208 | Tests password hash comparison |

#### PBKDF2 Compliance (`test_pbkdf2_iterations.c`)

| Test | Standard | Description |
|------|----------|-------------|
| NIST minimum iterations | NIST SP 800-132 | Verifies ≥100,000 iterations |
| OWASP recommendation | OWASP | Documents current recommendations |
| Performance balance | - | Tests iteration/performance trade-off |
| Hardcoded iterations | CWE-916 | Verifies constant iteration count |
| SEC-027 compliance | SEC-027 | Verifies upgrade to 100,000 iterations |

#### HMAC Integrity (`test_hmac_verification.c`)

| Test | CWE | Description |
|------|-----|-------------|
| HMAC computation | CWE-353 | Tests signature generation |
| HMAC verification | CWE-354 | Tests signature validation |
| Tamper detection | CWE-354 | Tests data modification detection |
| Wrong key detection | CWE-354 | Tests key mismatch detection |
| Salt uniqueness | - | Tests per-entry salt generation |

#### CLI Input Validation (`test_cli_input.c`)

| Test | CWE | Description |
|------|-----|-------------|
| Command injection | CWE-78 | Detects shell metacharacters |
| Path traversal | CWE-22 | Detects path traversal sequences |
| Input sanitization | CWE-20 | Tests input cleaning |
| Printable ASCII | CWE-20 | Validates character set |
| Numeric validation | CWE-20 | Tests number format validation |
| Numeric overflow | CWE-20 | Detects overflow attempts |

#### Config File Parsing (`test_config_parsing.c`)

| Test | CWE | Description |
|------|-----|-------------|
| Safe parsing | CWE-676 | Tests secure parsing implementation |
| Injection detection | CWE-78 | Detects config injection attempts |
| Key validation | CWE-20 | Validates configuration keys |
| Value validation | CWE-20 | Validates configuration values |
| Buffer protection | CWE-119 | Tests buffer overflow protection |
| Comment handling | - | Tests comment parsing |

---

## 4. CI/CD Integration

### 4.1 GitHub Actions Workflow

**File:** `.github/workflows/security.yml`

The workflow runs on:
- Every push to `main` or `develop`
- Every pull request to `main` or `develop`
- Weekly scheduled runs (Mondays 09:00 UTC)
- Manual triggering

#### Workflow Jobs

| Job | Tool | Purpose |
|-----|------|---------|
| `cppcheck-security` | Cppcheck | Static analysis security scan |
| `clang-tidy-security` | Clang-Tidy | Security-focused code analysis |
| `security-tests` | Custom | Security test suite execution |
| `security-summary` | - | Report aggregation |
| `codeql` | CodeQL | Advanced security analysis (scheduled only) |

#### Artifacts

| Artifact | Contents | Retention |
|----------|----------|-----------|
| `cppcheck-reports/` | Cppcheck analysis results | 30 days |
| `clang-tidy-reports/` | Clang-Tidy analysis results | 30 days |
| `security-test-reports/` | Security test execution logs | 30 days |
| `security-summary/` | Aggregated security report | 30 days |

### 4.2 Failure Conditions

| Severity | Action |
|----------|--------|
| **Error** | Build fails (must be fixed) |
| **Warning** | Reported, does not fail build |
| **Style** | Reported, informational only |

---

## 5. Running Security Tests

### 5.1 Quick Security Check

```bash
# Run all security checks
make security-check
```

### 5.2 Individual Security Scans

```bash
# Cppcheck only
make security-cppcheck

# Clang-tidy only
make security-clang-tidy

# Security tests only
make security-tests
```

### 5.3 Full Security Scan

```bash
# Clean and run complete scan
make security-scan-full

# Results in:
# - security-cppcheck-report.txt
# - security-clang-tidy-report.txt
# - security-test-reports/
```

### 5.4 Clean Security Reports

```bash
make security-clean
```

---

## 6. Interpreting Results

### 6.1 Cppcheck Output

```
src/cli_auth.c:45:8: warning: Variable 'buffer' is assigned a value that is never used [unreadVariable]
src/hal_pwm.c:120:5: error: Memory leak: data [memleak]
```

| Severity | Color | Action Required |
|----------|-------|-----------------|
| `error` | 🔴 Red | Must fix before commit |
| `warning` | 🟡 Yellow | Should fix, review required |
| `style` | 🟢 Green | Optional cleanup |
| `information` | ℹ️ Blue | Informational only |

### 6.2 Clang-Tidy Output

```
src/cli_auth.c:45:8: warning: Use of memory after it is freed [clang-analyzer-unix.Malloc]
src/hal_pwm.c:120:5: error: Null pointer dereference [clang-diagnostic-null]
```

| Check Prefix | Standard |
|--------------|----------|
| `cert-*` | CERT C Secure Coding |
| `security-*` | Security-specific |
| `clang-analyzer-*` | Clang static analyzer |
| `bugprone-*` | Bug-prone patterns |

### 6.3 Security Test Output

```
==============================================
AdaptivePWM Security Tests: Buffer Handling
CWE-120, CWE-121, CWE-122, CWE-158, CWE-190
==============================================

--- Test: Password Buffer Overflow Protection ---
  ✓ PASS: strlen(password_buffer) < MAX_PASSWORD_LEN
  ✓ PASS: password_buffer[MAX_PASSWORD_LEN - 1] == '\0'

Security Test Summary
==============================================
Tests run: 5
Failures: 0

✓ All security tests PASSED
```

---

## 7. Remediation Workflow

### 7.1 When Security Scan Fails

1. **Review Report**: Check `security-cppcheck-report.txt` or CI artifacts
2. **Triage Issues**: Classify as true positive or false positive
3. **Fix or Suppress**: Fix the issue or add to `ci/cppcheck-suppressions.txt` with justification
4. **Re-run**: Execute `make security-check` to verify fix
5. **Document**: Update this guide if new patterns emerge

### 7.2 Adding False Positive Suppressions

Edit `ci/cppcheck-suppressions.txt`:

```
# Justification: Hardware register access is intentional
noPointerArithmetic:*hal_*.c

# Justification: Documented safe pattern
dangerousFunction:src/safe_module.c:function_name
```

---

## 8. Security Test Development

### 8.1 Adding New Security Tests

1. Create new file in `tests/security/`: `test_<feature>_<vulnerability>.c`
2. Include relevant CWE references in file header
3. Follow existing test pattern with `TEST_ASSERT` macros
4. Update this documentation
5. Run `make security-tests` to verify

### 8.2 Test Template

```c
/**
 * @file test_feature_vulnerability.c
 * @brief Security tests for [feature] [vulnerability]
 * @details [description]
 * 
 * Security Framework:
 * - CWE-XXX: [CWE name]
 * - ISO 27001: [control]
 * - NIST CSF: [control]
 * 
 * @version 1.0.0
 * @date YYYY-MM-DD
 * @security SEC-038: Automated Security Testing
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>

// Minimal test framework
#define TEST_ASSERT(condition) do { \
    if (!(condition)) { \
        printf("  ✗ FAIL: %s (line %d)\n", #condition, __LINE__); \
        test_failures++; \
    } else { \
        printf("  ✓ PASS: %s\n", #condition); \
    } \
} while(0)

// Test functions...

int main(void) {
    printf("Test Header...\n");
    // Run tests...
    return test_failures;
}
```

---

## 9. Related Documentation

| Document | Description |
|----------|-------------|
| `PROJECT_FRAMEWORK.md` | Security and Safety Framework |
| `docs/safety.md` | Safety protocols and requirements |
| `docs/api.md` | API security considerations |
| `tests/test_cli_auth.c` | Authentication unit tests |
| `src/flash_logger_hmac.h` | HMAC implementation details |
| `src/cli_auth.c` | Authentication implementation |

---

## 10. References

### Standards
- [NIST SP 800-132](https://csrc.nist.gov/publications/detail/sp/800-132/final) - Password-Based Key Derivation
- [CERT C Secure Coding Standard](https://wiki.sei.cmu.edu/confluence/display/c/SEI+CERT+C+Coding+Standard) - Secure C coding
- [CWE Top 25](https://cwe.mitre.org/top25/) - Most dangerous software weaknesses
- [OWASP ASVS](https://owasp.org/www-project-application-security-verification-standard/) - Application Security Verification

### Tools
- [Cppcheck Manual](http://cppcheck.sourceforge.net/manual.html)
- [Clang-Tidy Checks](https://clang.llvm.org/extra/clang-tidy/checks/list.html)
- [CodeQL Documentation](https://codeql.github.com/docs/)

---

## 11. Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.0 | 2026-04-16 | Security Agent | Initial document created for SEC-038 |

---

*End of Security Testing Guide*
