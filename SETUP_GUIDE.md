# AdaptivePWM Developer Setup Guide

## Complete Framework Installation

**Version:** 2.1.0  
**Framework:** CISSP-Aligned Security  
**Date:** 2026-03-21

---

## Overview

This guide sets up the complete AdaptivePWM development environment with:
- ✅ Framework enforcement at build time
- ✅ Git hooks for pre-commit validation
- ✅ Security review templates
- ✅ Quick reference documentation

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

## Step 6: Review Documentation

**Required reading for all developers:**

1. **PROJECT_FRAMEWORK.md** - Governance and security requirements
2. **QUICK_REFERENCE.md** - Daily workflow and commands
3. **docs/design.md** - Clock system and architecture

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

---

## File Organization

```
AdaptivePWM/
├── PROJECT_FRAMEWORK.md      # ← READ THIS
├── QUICK_REFERENCE.md        # ← Daily reference
├── CHANGELOG.md              # ← Update per change
├── README.md                 # ← Project overview
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
│   ├── config.h             # ← Central config
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

## Success Criteria

Before you start developing, verify:

- [ ] Git hooks installed (`ls .git/hooks/pre-commit`)
- [ ] Framework check passes (`./ci/enforce_framework.sh`)
- [ ] Build succeeds (`pio run -e nucleo_f401re`)
- [ ] Read PROJECT_FRAMEWORK.md
- [ ] Read QUICK_REFERENCE.md
- [ ] Read docs/design.md (clock system)

---

## Support

| Resource | Purpose |
|----------|---------|
| QUICK_REFERENCE.md | Daily commands and workflow |
| PROJECT_FRAMEWORK.md | Governance and requirements |
| docs/design.md | Architecture details |
| docs/api.md | API reference |
| docs/safety.md | Security protocols |

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
□ Ready to develop!
```

---

**Next Steps:**
1. Make a small test change
2. Update documentation
3. Commit (test hooks)
4. Verify framework compliance
5. Start real development

**Questions?** See QUICK_REFERENCE.md or PROJECT_FRAMEWORK.md
