# AdaptivePWM Quick Reference

**Project:** AdaptivePWM - Real-time PWM Control System  
**Version:** 2.1.0  
**Framework:** CISSP-Aligned Security  
**Clock:** 16 MHz HSE → 84 MHz SYSCLK

---

## 🚀 Quick Start

```bash
# Clone and setup
git clone <repository>
cd AdaptivePWM

# Install git hooks (REQUIRED)
./git-hooks/install.sh

# Build release version
pio run -e nucleo_f401re

# Build with framework check
pio run -e nucleo_f401re --target pre:ci/framework_check.py

# Flash to device
pio run -e nucleo_f401re --target upload

# Monitor serial
pio device monitor -b 115200
```

---

## 📋 Daily Workflow

### 1. Before Coding

```bash
# Check current status
./ci/enforce_framework.sh

# Review framework if needed
less PROJECT_FRAMEWORK.md
```

### 2. Making Changes

```bash
# Create feature branch
git checkout -b feature/my-change

# Edit code + documentation together
# See: Framework Requirement 4.3
```

### 3. Pre-Commit (Automated)

```bash
git add src/myfile.c docs/api.md
git commit -m "feat(pwm): add feature description"

# Hooks automatically run:
# ✓ Framework compliance check
# ✓ Security scan
# ✓ Documentation freshness
# ✓ Commit message format
```

### 4. If Commit Blocked

```bash
# See errors, fix them
./ci/enforce_framework.sh

# Or bypass in emergencies (not recommended)
git commit --no-verify
```

---

## 🛡️ Security Checklist (Per Change)

### Before Commit

```
□ Input validation added (SR-003)
□ NULL pointer checks present
□ Buffer bounds checked
□ No hardcoded secrets
□ Watchdog refresh preserved
□ Error paths tested
□ Documentation updated (Framework 4.3)
```

### CISSP Lens Questions

1. **Domain 3 (Architecture):** Does this change affect clock, watchdog, or fail-safe behavior?
2. **Domain 8 (DevSec):** Are all inputs validated? Are outputs encoded?
3. **Domain 6 (Assessment):** Can this code be fuzz tested? Are there new attack surfaces?

---

## 🔧 Common Tasks

### Clock Configuration

```c
// Current: 16 MHz HSE → 84 MHz SYSCLK
// PLL: M=16, N=336, P=4, Q=7

// Get current clock frequencies
uint32_t sysclk = HAL_RCC_GetSysClockFreq();  // 84 MHz
uint32_t hclk = HAL_RCC_GetHCLKFreq();        // 84 MHz
uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();      // 42 MHz
uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();     // 84 MHz
```

### PWM Usage

```c
// Initialize
Adaptive_PWM_t pwm;
Adaptive_PWM_Init(&pwm);
Adaptive_PWM_Start(&pwm);

// Set duty (0.0 - 1.0, clamped to 0.02-0.98)
Adaptive_PWM_SetDuty(&pwm, 0.5f);

// Emergency stop
Adaptive_PWM_EmergencyStop(&pwm);
```

### ADC Usage

```c
// Initialize (42 MHz ADC clock)
Adaptive_ADC_t adc;
Adaptive_ADC_Init(&adc);
Adaptive_ADC_Start_DMA(&adc);

// Process in DMA callback
Adaptive_ADC_ProcessBuffer(&adc);

// Read measurement
ADC_Measurement_t meas;
if (Adaptive_ADC_GetMeasurement(&adc, &meas)) {
    float vin = meas.vin;
    float vout = meas.vout;
    float current = meas.current;
}
```

### Safety Critical

```c
// Always include in main loop
Adaptive_WDG_Refresh();  // Every <100ms

// Check clock security
if (HAL_RCC_GetSYSCLKSource() != RCC_SYSCLKSOURCE_PLLCLK) {
    Error_Handler();
}
```

---

## 📊 Framework Compliance

### Quick Compliance Check

```bash
# Full framework check
./ci/enforce_framework.sh

# Python-based check (faster)
python3 ci/framework_check.py
```

### Documentation Requirements

| Change Type | Documentation Required |
|-------------|------------------------|
| Bug fix | CHANGELOG.md |
| New feature | API docs + CHANGELOG.md |
| Clock/Architecture | design.md + security review |
| Security feature | SECURITY_REVIEW_TEMPLATE.md |

### Commit Message Format

```
type(scope): subject

body (what and why)

Breaking Changes: (if any)
Security: (impact)
Documentation: (what updated)
```

**Types:** `feat`, `fix`, `docs`, `security`, `refactor`, `test`, `chore`, `ci`

**Scopes:** `pwm`, `adc`, `clock`, `cli`, `safety`, `hal`, `docs`

**Example:**
```
security(pwm): add duty cycle hard limits

Implement 2%-98% hardware limits as per SR-003.
Prevents complete shutdown and excessive stress.

Breaking Changes: None
Security: Adds defense in depth for PWM
Documentation: Updated api.md and safety.md
```

---

## 🧪 Testing

### Unit Tests

```bash
# Run all tests
pio test -e nucleo_f401re_test

# Run specific test
pio test -e nucleo_f401re_test --filter test_adc
```

### Security Tests

```bash
# Static analysis
cppcheck --enable=all src/

# Check for secrets
grep -r "password\|secret\|key=" src/ --include="*.c"

# Full security scan
./ci/enforce_framework.sh
```

### Hardware Tests

```bash
# Clock verification (needs scope on PA8/MCO)
# Expected: 84 MHz on MCO if configured

# ADC accuracy test
# Apply known voltages, verify readings

# PWM output test
# Scope on PA8/PA9 for complementary signals
```

---

## 🔍 Debugging

### Common Issues

| Issue | Check | Solution |
|-------|-------|----------|
| Build fails | Framework compliance | Run `./ci/enforce_framework.sh` |
| Commit blocked | Documentation | Update docs per change |
| Watchdog reset | Refresh interval | Call `Adaptive_WDG_Refresh()` more often |
| Clock wrong | HSE crystal | Verify 16 MHz crystal |
| ADC noisy | Grounding | Check analog ground connections |

### Debug Build

```bash
# Build with debug symbols
pio run -e nucleo_f401re_debug

# Start GDB
pio debug -e nucleo_f401re_debug
```

### CLI Commands (Debug Build)

```
status          - Show system status
status adc      - ADC readings
status pwm      - PWM configuration
config          - Change parameters
monitor         - Real-time monitoring
pwm 0.5         - Set 50% duty
pwm start       - Start PWM
pwm stop        - Stop PWM
errors          - Show error log
help            - Show help
```

---

## 📚 Documentation Map

```
docs/
├── index.md              # Start here
├── design.md             # Clock system architecture
├── api.md                # API reference
└── safety.md             # Security protocols

PROJECT_FRAMEWORK.md      # Governance (read this!)
QUICK_REFERENCE.md        # This file
CHANGELOG.md              # Change history

security/
└── SECURITY_REVIEW_TEMPLATE.md  # For security changes

ci/
├── enforce_framework.sh   # Main compliance check
└── framework_check.py     # Python version (faster)

git-hooks/
├── install.sh            # Setup hooks
├── pre-commit            # Pre-commit checks
└── README.md             # Hook documentation
```

---

## 🆘 Emergency Procedures

### Framework Violation

1. **Don't panic** - Fix violations systematically
2. **Read error** - `./ci/enforce_framework.sh` shows what's wrong
3. **Fix issues** - Usually documentation or security checks
4. **Re-run** - Verify fix with `./ci/enforce_framework.sh`

### Security Incident

1. **Assess impact** - What could be affected?
2. **Document** - Use SECURITY_REVIEW_TEMPLATE.md
3. **Fix** - Implement remediation
4. **Review** - Security officer must approve
5. **Deploy** - With emergency procedures

### Clock System Failure

```c
// Symptoms: System runs slow/erratic
// Check:
if (HAL_RCC_GetSYSCLKSource() != RCC_SYSCLKSOURCE_PLLCLK) {
    // HSE failed, running on HSI (16 MHz)
    // System still works but slower
    Error_Report(&error_manager, ERR_CLOCK_FAIL, ...);
}
```

---

## 📞 Getting Help

| Resource | Location |
|----------|----------|
| Framework | `PROJECT_FRAMEWORK.md` |
| Architecture | `docs/design.md` |
| Security | `docs/safety.md` |
| API | `docs/api.md` |
| Quick Ref | `QUICK_REFERENCE.md` (this file) |

---

## ✅ Framework Check Summary

Run this before every commit:

```bash
./ci/enforce_framework.sh
```

Expected output:
```
✓ Documentation: index.md
✓ Documentation: design.md
✓ Documentation: api.md
✓ Documentation: safety.md
✓ SR-001: CSS (HSE) configured
✓ SR-002: Watchdog initialized
✓ SR-003: PWM boundary validation present
✓ NULL pointer validation adequate
✓ No hardcoded secrets detected
...
✓ FRAMEWORK COMPLIANCE: PASSED
```

---

**Last Updated:** 2026-03-21  
**Framework Version:** 1.0.0
