#!/usr/bin/env python3
#
# AdaptivePWM Framework Check - PlatformIO Pre-Build Script
# CISSP-Aligned Security and Documentation Verification
#

import subprocess
import sys
import os

# ANSI colors for terminal output
RED = '\033[0;31m'
GREEN = '\033[0;32m'
YELLOW = '\033[1;33m'
NC = '\033[0m'  # No Color

def log_ok(msg):
    print(f"{GREEN}✓{NC} {msg}")

def log_warn(msg):
    print(f"{YELLOW}⚠{NC} {msg}")

def log_error(msg):
    print(f"{RED}✗{NC} {msg}")

def run_framework_check():
    """Run the bash framework enforcement script"""
    script_path = os.path.join(os.path.dirname(__file__), 'enforce_framework.sh')
    
    if not os.path.exists(script_path):
        log_error("Framework enforcement script not found!")
        return False
    
    try:
        result = subprocess.run(
            ['bash', script_path],
            capture_output=True,
            text=True,
            timeout=30
        )
        
        # Print output
        print(result.stdout)
        
        if result.returncode != 0:
            log_error("Framework compliance check FAILED")
            if result.stderr:
                print(result.stderr)
            return False
        
        log_ok("Framework compliance check PASSED")
        return True
        
    except subprocess.TimeoutExpired:
        log_error("Framework check timed out")
        return False
    except Exception as e:
        log_error(f"Framework check error: {e}")
        return False

def check_clock_configuration():
    """Verify clock configuration is correct"""
    main_c = os.path.join(os.path.dirname(__file__), '..', 'src', 'main.c')
    
    if not os.path.exists(main_c):
        log_error("main.c not found!")
        return False
    
    with open(main_c, 'r') as f:
        content = f.read()
    
    checks = [
        ('RCC_HSE_ON', 'HSE clock enabled'),
        ('PLLM = 16', 'PLL multiplier configured'),
        ('Adaptive_WDG_Init', 'Watchdog initialized'),
        ('SystemClock_Config', 'Clock configuration present')
    ]
    
    all_pass = True
    for pattern, desc in checks:
        if pattern in content:
            log_ok(f"Clock: {desc}")
        else:
            log_error(f"Clock: {desc} - MISSING")
            all_pass = False
    
    return all_pass

def check_documentation():
    """Verify documentation exists and is current"""
    docs_dir = os.path.join(os.path.dirname(__file__), '..', 'docs')
    required_docs = [
        'index.md',
        'design.md',
        'api.md',
        'safety.md'
    ]
    
    all_exist = True
    for doc in required_docs:
        path = os.path.join(docs_dir, doc)
        if os.path.exists(path):
            log_ok(f"Documentation: {doc}")
        else:
            log_error(f"Documentation: {doc} - MISSING")
            all_exist = False
    
    return all_exist

def check_changelog():
    """Verify CHANGELOG.md exists"""
    changelog = os.path.join(os.path.dirname(__file__), '..', 'CHANGELOG.md')
    if os.path.exists(changelog):
        log_ok("CHANGELOG.md exists")
        return True
    else:
        log_error("CHANGELOG.md - MISSING")
        return False

def check_framework_doc():
    """Verify PROJECT_FRAMEWORK.md exists"""
    framework = os.path.join(os.path.dirname(__file__), '..', 'PROJECT_FRAMEWORK.md')
    if os.path.exists(framework):
        log_ok("PROJECT_FRAMEWORK.md exists")
        return True
    else:
        log_error("PROJECT_FRAMEWORK.md - MISSING")
        return False

def main():
    """Main pre-build check"""
    print("=" * 60)
    print("AdaptivePWM Framework Pre-Build Check")
    print("CISSP-Aligned Security Verification")
    print("=" * 60)
    print()
    
    # Run all checks
    checks = [
        ("Documentation", check_documentation),
        ("Changelog", check_changelog),
        ("Framework", check_framework_doc),
        ("Clock Configuration", check_clock_configuration),
    ]
    
    results = []
    for name, check_func in checks:
        print(f"\n{name}:")
        result = check_func()
        results.append((name, result))
    
    # Run full framework check
    print(f"\nFull Framework Check:")
    full_result = run_framework_check()
    results.append(("Full Framework", full_result))
    
    # Summary
    print()
    print("=" * 60)
    all_passed = all(result for _, result in results)
    
    if all_passed:
        log_ok("ALL FRAMEWORK CHECKS PASSED")
        print("=" * 60)
        return 0
    else:
        log_error("FRAMEWORK CHECKS FAILED")
        print("\nFailed checks:")
        for name, result in results:
            if not result:
                print(f"  - {name}")
        print("=" * 60)
        return 1

if __name__ == "__main__":
    sys.exit(main())

# PlatformIO Import - this must be at the end
Import("env")

# Run checks before build
print("\n[Pre-Build] Running Framework Compliance Check...")
result = main()
if result != 0:
    raise Exception("Framework compliance check failed. Build aborted.")
print("[Pre-Build] Framework check completed.\n")
