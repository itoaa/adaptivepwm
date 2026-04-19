# AdaptivePWM Developer Setup Guide

## Complete Framework Installation

**Version:** 2.3.1  
**Framework:** CISSP-Aligned Security  
**Date:** 2026-04-16

---

## Overview

This guide sets up the complete AdaptivePWM development environment with:
- ✅ Framework enforcement at build time
- ✅ Git hooks for pre-commit validation
- ✅ Security review templates
- ✅ Quick reference documentation
- ✅ **Physical confirmation for first-time setup (SEC-031)**

---

## Step 1: Prerequisites

```bash
# Required software
- PlatformIO Core
- Git
- Python 3.8+
- Bash (Linux/Mac) or Git Bash (Windows)

# Verify installations
platformio --version
git --version
python3 --version
```

---

## Step 2: Clone Repository

```bash
git clone <adaptivepwm-repository-url>
cd AdaptivePWM
```

---

## Step 3: Install Git Hooks (REQUIRED)

This is **mandatory** for all developers:

```bash
./git-hooks/install.sh
```

**What this installs:**
- `pre-commit` - Framework compliance check
- `commit-msg` - Message format validation
- `post-checkout` - Branch switch reminder
- `post-merge` - Post-merge framework check

**Verify installation:**
```bash
ls -la .git/hooks/pre-commit
ls -la .git/hooks/commit-msg
```

---

## Step 4: Verify Framework Setup

```bash
# Run full framework check
./ci/enforce_framework.sh
```

**Expected output:**
```
=====================================
AdaptivePWM Framework Enforcement
CISSP-Aligned Security Checks
=====================================
...
✓ FRAMEWORK COMPLIANCE: PASSED
=====================================
```

**If you see errors:**
1. Read the error messages
2. Fix the issues (usually documentation)
3. Re-run until PASSED

---

## Step 5: Build Test

```bash
# Test build with framework check
pio run -e nucleo_f401re
```

**What happens:**
1. Framework check runs automatically (via extra_scripts)
2. Code compiles with security flags
3. Binary size checked
4. Build artifacts created

**Expected:** `SUCCESS` with no warnings

---

## Step 6: Hardware Setup (STM32F401RE)

### Physical Confirmation Setup (SEC-031)

**Security Feature:** First-time password setup requires physical confirmation to prevent remote attackers from setting passwords before the legitimate owner gains physical access.

#### Required Hardware

| Component | Default Pin | Alternative | Mode |
|-----------|-------------|-------------|------|
| Setup Button | PA0 | - | Button mode |
| Setup Jumper | PA8 | PA0 | Jumper mode |

#### Wiring Diagram

```
STM32F401RE Nucleo Board

PA0 (Button mode):
    ┌─────────────────┐
    │ PA0 ────┬───┐  │
    │         │   │  │
    │      [BUTTON]  │
    │         │   │  │
    │         └───┘  │
    │           │    │
    └───────────┼────┘
                │
               GND

PA8 (Jumper mode):
    ┌─────────────────┐
    │ PA8 ────┬───┐  │
    │         │ J │  │
    │         │ U │  │
    │         │ M │  │
    │         │ P │  │
    │         │ E │  │
    │         │ R │  │
    │         └───┘  │
    │           │    │
    └───────────┼────┘
                │
               GND

Configuration:
- Pull-up enabled internally
- Button press or jumper connect = pin goes LOW
- 50ms debounce for button
- 100ms minimum press duration required
```

#### Setup Modes (config.h)

```c
// Configure in src/config.h

// Mode selection:
#define SETUP_MODE_NONE      0   // Disabled (dev only)
#define SETUP_MODE_BUTTON    1   // PA0 button
#define SETUP_MODE_JUMPER    2   // PA8 jumper
#define SETUP_MODE_BOTH      3   // Either button or jumper

// Default (recommended): BUTTON mode
#ifndef SETUP_CONFIRM_MODE
    #define SETUP_CONFIRM_MODE SETUP_MODE_BUTTON
#endif

// Timeout: 30 seconds
#ifndef SETUP_TIMEOUT_MS
    #define SETUP_TIMEOUT_MS 30000
#endif

// Button debounce: 50ms
#ifndef SETUP_BUTTON_DEBOUNCE_MS
    #define SETUP_BUTTON_DEBOUNCE_MS 50
#endif

// Minimum press: 100ms
#ifndef SETUP_BUTTON_PRESS_MIN_MS
    #define SETUP_BUTTON_PRESS_MIN_MS 100
#endif
```

#### First-Time Setup Procedure

**When password is not set:**

1. **Connect hardware:**
   - Connect PA0 to button (for BUTTON mode)
   - Or connect PA8 to jumper (for JUMPER mode)

2. **Power on the device:**
   ```bash
   pio run --target upload
   ```

3. **First login attempt:**
   ```
   login mypassword123
   ```

4. **System response:**
   ```
   *** FIRST-TIME SETUP ***
   Physical confirmation required for security.
   Mode: BUTTON (PA0)

   Please press and hold the setup button...
   Timeout: 30 seconds
   ```

5. **Physical confirmation:**
   - Press and hold button for at least 100ms
   - Or install jumper (for JUMPER mode)

6. **Success:**
   ```
   Physical confirmation received!
   Setting initial password...
   Password set successfully!
   Jumper can be removed now.
   Authentication successful
   ```

#### Security Rationale

**Without physical confirmation:**
- Remote attacker could set password if they gain UART access before owner
- No way to verify legitimate owner has physical device

**With physical confirmation:**
- Attacker must have physical access to device
- Legitimate owner must press button/install jumper
- Creates "air gap" for initial setup

**Framework Compliance:**
- CISSP Domain 5: Identity and Access Management
- NIST CSF 2.0: PR.AC-01 (Access Control)
- ISO 27001:2022: A.9.4 (Access control)
- Security Assessment: ADP-IAM-001 (CVSS 5.3 - MEDIUM)

#### Troubleshooting Setup Confirmation

**Problem: "Physical confirmation timeout"**

**Causes:**
- Button not connected properly
- Jumper not installed (for JUMPER mode)
- Wrong GPIO pin configuration
- Timeout too short

**Solutions:**
1. Check wiring with multimeter
2. Verify pin state: `authstatus` command
3. Check mode in config.h
4. Increase timeout if needed

**Problem: Button bounces / false triggers**

**Solutions:**
- Increase debounce time: `SETUP_BUTTON_DEBOUNCE_MS`
- Use hardware debounce (RC filter)
- Check button quality

**Problem: Want to disable for development**

**Option 1: Disable in build**
```bash
pio run -e nucleo_f401re --build-flag="-DSETUP_CONFIRM_MODE=SETUP_MODE_NONE"
```

**Option 2: Edit config.h (temporary)**
```c
#define SETUP_CONFIRM_MODE SETUP_MODE_NONE
```

**⚠️ Never disable in production builds!**

---

## Step 7: Review Documentation

**Required reading for all developers:**

1. **PROJECT_FRAMEWORK.md** - Governance and security requirements
2. **QUICK_REFERENCE.md** - Daily workflow and commands
3. **docs/design.md** - Clock system and architecture
4. **SETUP_GUIDE.md** - This file (physical confirmation)

```bash
# Quick review
less PROJECT_FRAMEWORK.md
less QUICK_REFERENCE.md
```

---

## Development Workflow

### Daily Workflow

```bash
# 1. Check framework status
./ci/enforce_framework.sh

# 2. Make changes
# Edit code + documentation together!

# 3. Build and test
pio run -e nucleo_f401re

# 4. Commit (hooks run automatically)
git add .
git commit -m "feat(scope): description"

# 5. If hooks block, fix and retry
# Don't use --no-verify unless emergency!
```

### Commit Message Format

```
type(scope): subject

body explaining what and why

Breaking Changes: (if any)
Security: (impact assessment)
Documentation: (what was updated)
```

**Types:** `feat`, `fix`, `docs`, `security`, `refactor`, `test`, `chore`, `ci`

**Example:**
```
feat(adc): add 16MHz HSE clock support

Implement optimized clock configuration with:
- HSE: 16 MHz external crystal
- PLL: M=16, N=336, P=4, Q=7
- SYSCLK: 84 MHz
- ADC Clock: 42 MHz (maximum)

Breaking Changes: None
Security: SR-001 CSS enabled
Documentation: Updated design.md and api.md
```

---

## Security Review Process

### For Critical/Major Changes

1. **Before coding** - Create security review:
```bash
# Copy template
cp docs/security/SECURITY_REVIEW_TEMPLATE.md \
   docs/security/reviews/SEC-YYYY-MM-DD-my-change.md

# Fill out template
```

2. **Get approvals** before implementation:
- Security Officer (CISSP-aligned review)
- Technical Lead
- Project Owner (for critical changes)

3. **Implement** with security in mind

4. **Verify** with framework check

---

## Framework Enforcement

### What Gets Checked

| Check | Tool | Trigger |
|-------|------|---------|
| Documentation exists | Bash script | Every commit |
| Documentation freshness | Git diff | Every commit |
| Security requirements | Code grep | Every commit |
| Hardcoded secrets | Pattern match | Every commit |
| Code structure | File naming | Every commit |
| Full framework | `./ci/enforce_framework.sh` | Pre-commit hook |

### Build Integration

The framework check is integrated into PlatformIO builds:

```ini
# platformio.ini
extra_scripts = pre:ci/framework_check.py
```

This means:
- Every build runs framework check
- Build fails if framework violated
- Forces compliance

### Bypass (Emergency Only)

```bash
# Bypass pre-commit hooks
git commit --no-verify

# ⚠️ Use only in genuine emergencies
# ⚠️ Must fix framework issues before push
# ⚠️ Document reason in commit message
```

---

## Troubleshooting

### Issue: "Framework compliance check failed"

**Solution:**
```bash
# Read specific error
./ci/enforce_framework.sh

# Fix issues (usually):
# 1. Missing documentation → Create/update docs
# 2. Security violation → Add validation
# 3. Code structure → Rename files

# Re-check until passed
./ci/enforce_framework.sh
```

### Issue: "Git hooks not running"

**Solution:**
```bash
# Verify hooks are executable
ls -la .git/hooks/pre-commit

# If not, reinstall
./git-hooks/install.sh

# Check git config
git config --get core.hooksPath
# Should be empty or .git/hooks
```

### Issue: "Build fails with framework error"

**Solution:**
```bash
# Framework check runs before build
# Check output for specific violation

# Common fixes:
# - Update docs/ if src/ changed
# - Add NULL checks if missing
# - Remove hardcoded secrets
# - Update CHANGELOG.md
```

### Issue: "Want to disable hooks temporarily"

**Solution:**
```bash
# Rename hooks directory
mv .git/hooks .git/hooks-disabled

# Work...

# Restore hooks
mv .git/hooks-disabled .git/hooks
```

**⚠️ Never push code that bypasses framework!**

### Issue: "Physical confirmation not working"

**Solution:**
```bash
# 1. Check auth status
authstatus
# Should show: Setup confirmation: BUTTON (PA0)

# 2. Verify pin configuration
cat src/config.h | grep SETUP_CONFIRM

# 3. Test with debug build
pio run -e nucleo_f401re_debug

# 4. Check wiring with multimeter
# PA0 or PA8 should be HIGH when open
# Should go LOW when button pressed/jumper installed

# 5. For development only, disable:
# Add to platformio.ini build_flags:
# -DSETUP_CONFIRM_MODE=SETUP_MODE_NONE
```

---

## Available Commands Reference

### Framework

```bash
./ci/enforce_framework.sh          # Full compliance check
python3 ci/framework_check.py       # Python version (faster)
./git-hooks/install.sh              # Install/reset git hooks
```

### Build

```bash
pio run -e nucleo_f401re            # Release build
pio run -e nucleo_f401re_debug      # Debug build
pio run -e nucleo_f401re_test       # Test build
pio test -e nucleo_f401re_test      # Run tests
pio run --target upload             # Flash device
pio device monitor -b 115200        # Serial monitor
```

### Git

```bash
git commit -m "feat(scope): desc"     # Normal commit (hooks run)
git commit --no-verify                # Bypass hooks (emergency)
git log --oneline -10                 # Recent commits
```

### Security

```bash
# Check auth status
authstatus

# Login
login <password>

# Change password (when authenticated)
passwd <new_password>

# Logout
logout
```

---

## File Organization

```
AdaptivePWM/
├── PROJECT_FRAMEWORK.md      # ← READ THIS
├── QUICK_REFERENCE.md        # ← Daily reference
├── CHANGELOG.md              # ← Update per change
├── README.md                 # ← Project overview
├── SETUP_GUIDE.md            # ← This file (physical confirmation)
├── platformio.ini            # ← Build config (includes framework check)
│
├── docs/                     # ← Documentation (REQUIRED)
│   ├── index.md
│   ├── design.md            # ← Clock system
│   ├── api.md               # ← API reference
│   ├── safety.md            # ← Security protocols
│   └── security/            # ← Security reviews
│       └── SECURITY_REVIEW_TEMPLATE.md
│
├── src/                      # ← Source code
│   ├── main.c               # ← Entry point + clock config
│   ├── config.h             # ← Central config (physical confirmation settings)
│   ├── cli_auth.c/h         # ← Authentication (SEC-031)
│   ├── setup_gpio.c/h       # ← Physical confirmation (SEC-031)
│   └── hal_*.c/h            # ← HAL drivers
│
├── ci/                       # ← CI/CD scripts
│   ├── enforce_framework.sh # ← Main compliance script
│   └── framework_check.py     # ← Python version
│
└── git-hooks/                # ← Hook templates
    ├── install.sh           # ← Setup script
    ├── pre-commit           # ← Pre-commit checks
    └── README.md
```

---

## Security Feature Summary

### Implemented Features

| Feature | Status | Reference |
|---------|--------|-----------|
| UART CLI Authentication | ✅ | SEC-019 |
| PBKDF2-SHA256 (100K iter) | ✅ | SEC-027 |
| Hardware RNG (STM32 TRNG) | ✅ | SEC-033 |
| **Physical Confirmation** | ✅ | **SEC-031** |
| Account Lockout | ✅ | - |
| Session Timeout | ✅ | - |

### Physical Confirmation Details

**Configuration Options:**
```c
// src/config.h

SETUP_CONFIRM_ENABLED       // 1 = enabled, 0 = disabled
SETUP_CONFIRM_MODE          // BUTTON, JUMPER, or BOTH
SETUP_CONFIRM_GPIO_PORT     // GPIOA (default)
SETUP_CONFIRM_GPIO_PIN      // PA0 (default button)
SETUP_CONFIRM_ALT_GPIO_PIN  // PA8 (default jumper)
SETUP_TIMEOUT_MS            // 30000 (30 seconds)
SETUP_BUTTON_DEBOUNCE_MS    // 50 (milliseconds)
SETUP_BUTTON_PRESS_MIN_MS   // 100 (milliseconds)
```

**Security Benefits:**
- Prevents remote password setting
- Requires physical presence
- Configurable for different hardware
- Timeout protection
- Debounced input for reliability

---

## Success Criteria

Before you start developing, verify:

- [ ] Git hooks installed (`ls .git/hooks/pre-commit`)
- [ ] Framework check passes (`./ci/enforce_framework.sh`)
- [ ] Build succeeds (`pio run -e nucleo_f401re`)
- [ ] Read PROJECT_FRAMEWORK.md
- [ ] Read QUICK_REFERENCE.md
- [ ] Read docs/design.md (clock system)
- [ ] Read this SETUP_GUIDE.md (physical confirmation)
- [ ] Hardware setup understood (PA0/PA8)

---

## Checklist: New Developer Setup

```
□ Installed PlatformIO
□ Cloned repository
□ Ran ./git-hooks/install.sh
□ Verified hooks: ls .git/hooks/
□ Ran ./ci/enforce_framework.sh (should pass)
□ Ran pio run -e nucleo_f401re (should succeed)
□ Read PROJECT_FRAMEWORK.md
□ Read QUICK_REFERENCE.md
□ Read docs/design.md
□ Read SETUP_GUIDE.md (this file)
□ Understood physical confirmation wiring
□ Ready to develop!
```

---

## Support

| Resource | Purpose |
|----------|---------|
| QUICK_REFERENCE.md | Daily commands and workflow |
| PROJECT_FRAMEWORK.md | Governance and requirements |
| docs/design.md | Architecture details |
| docs/api.md | API reference |
| docs/safety.md | Security protocols |
| SETUP_GUIDE.md | This file (physical confirmation) |

---

**Next Steps:**
1. Make a small test change
2. Update documentation
3. Commit (test hooks)
4. Verify framework compliance
5. Test physical confirmation on hardware
6. Start real development

**Questions?** See QUICK_REFERENCE.md or PROJECT_FRAMEWORK.md
