#!/bin/bash
#
# AdaptivePWM Framework Enforcement Script
# CISSP-aligned security and documentation checks
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
FAILED=0

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "====================================="
echo "AdaptivePWM Framework Enforcement"
echo "CISSP-Aligned Security Checks"
echo "====================================="
echo ""

# Function to check if documentation exists
check_docs_exist() {
    local required_docs=(
        "docs/index.md"
        "docs/design.md"
        "docs/api.md"
        "docs/safety.md"
        "PROJECT_FRAMEWORK.md"
    )
    
    for doc in "${required_docs[@]}"; do
        if [[ ! -f "$PROJECT_ROOT/$doc" ]]; then
            echo -e "${RED}✗ Missing required documentation: $doc${NC}"
            FAILED=1
        else
            echo -e "${GREEN}✓ Found: $doc${NC}"
        fi
    done
}

# Function to check documentation freshness
check_docs_freshness() {
    echo ""
    echo "Checking documentation freshness..."
    
    # Get last commit date
    local last_commit_date=$(git -C "$PROJECT_ROOT" log -1 --format=%ct 2>/dev/null || echo "0")
    local current_time=$(date +%s)
    local days_since_commit=$(( (current_time - last_commit_date) / 86400 ))
    
    # Check if docs were modified in last commit
    local docs_modified=$(git -C "$PROJECT_ROOT" diff --name-only HEAD~1..HEAD 2>/dev/null | grep -c "^docs/" || echo "0")
    local code_modified=$(git -C "$PROJECT_ROOT" diff --name-only HEAD~1..HEAD 2>/dev/null | grep -c "^src/" || echo "0")
    
    if [[ $code_modified -gt 0 && $docs_modified -eq 0 ]]; then
        echo -e "${YELLOW}⚠ WARNING: Code changed but documentation not updated${NC}"
        echo -e "${YELLOW}  → Framework Requirement 4.3: Documentation must be updated with code changes${NC}"
        FAILED=1
    else
        echo -e "${GREEN}✓ Documentation synchronization OK${NC}"
    fi
}

# Function to check security requirements
check_security_requirements() {
    echo ""
    echo "Running CISSP-aligned security checks..."
    
    # Check for CSS (Clock Security System)
    if ! grep -q "RCC_HSE_ON" "$PROJECT_ROOT/src/main.c"; then
        echo -e "${RED}✗ SR-001 VIOLATION: CSS (HSE) not properly configured${NC}"
        FAILED=1
    else
        echo -e "${GREEN}✓ SR-001: CSS (HSE) configured${NC}"
    fi
    
    # Check for watchdog
    if ! grep -q "Adaptive_WDG_Init" "$PROJECT_ROOT/src/main.c"; then
        echo -e "${RED}✗ SR-002 VIOLATION: Watchdog not initialized${NC}"
        FAILED=1
    else
        echo -e "${GREEN}✓ SR-002: Watchdog initialized${NC}"
    fi
    
    # Check for boundary validation in PWM
    if ! grep -q "PWM_HARD_MIN_DUTY" "$PROJECT_ROOT/src/hal_pwm.c"; then
        echo -e "${RED}✗ SR-003 VIOLATION: PWM boundary validation missing${NC}"
        FAILED=1
    else
        echo -e "${GREEN}✓ SR-003: PWM boundary validation present${NC}"
    fi
    
    # Check for NULL pointer validation
    local null_checks=$(grep -c "== NULL" "$PROJECT_ROOT/src/hal"*.c 2>/dev/null || echo "0")
    if [[ $null_checks -lt 10 ]]; then
        echo -e "${YELLOW}⚠ WARNING: Limited NULL pointer checks detected${NC}"
    else
        echo -e "${GREEN}✓ NULL pointer validation adequate${NC}"
    fi
    
    # Check for hardcoded secrets (security anti-pattern)
    if grep -r "password\|secret\|key\s*=" "$PROJECT_ROOT/src/" 2>/dev/null | grep -v "// " | head -1; then
        echo -e "${RED}✗ SECURITY VIOLATION: Potential hardcoded secret found${NC}"
        FAILED=1
    else
        echo -e "${GREEN}✓ No hardcoded secrets detected${NC}"
    fi
}

# Function to check code structure
check_code_structure() {
    echo ""
    echo "Checking code structure compliance..."
    
    # Check file naming conventions
    local bad_names=$(find "$PROJECT_ROOT/src" -name "*.c" -o -name "*.h" 2>/dev/null | grep -v "^[a-z_]*\." || true)
    if [[ -n "$bad_names" ]]; then
        echo -e "${YELLOW}⚠ WARNING: Non-compliant file names:${NC}"
        echo "$bad_names"
    else
        echo -e "${GREEN}✓ File naming conventions OK${NC}"
    fi
    
    # Check function documentation (Doxygen style)
    local undocumented=$(grep -L "^/\*\*" "$PROJECT_ROOT/src/"*.h 2>/dev/null | wc -l)
    if [[ $undocumented -gt 0 ]]; then
        echo -e "${YELLOW}⚠ WARNING: $undocumented header files lack Doxygen documentation${NC}"
    else
        echo -e "${GREEN}✓ Header documentation adequate${NC}"
    fi
    
    # Check config.h exists and is used
    if [[ ! -f "$PROJECT_ROOT/src/config.h" ]]; then
        echo -e "${RED}✗ STRUCTURE VIOLATION: config.h missing${NC}"
        FAILED=1
    else
        echo -e "${GREEN}✓ Central configuration present${NC}"
    fi
}

# Function to check change documentation
check_change_documentation() {
    echo ""
    echo "Checking change management compliance..."
    
    # Check for CHANGELOG.md
    if [[ ! -f "$PROJECT_ROOT/CHANGELOG.md" ]]; then
        echo -e "${YELLOW}⚠ WARNING: CHANGELOG.md not found${NC}"
    else
        echo -e "${GREEN}✓ CHANGELOG.md present${NC}"
    fi
    
    # Check recent commit messages follow format
    local bad_commits=$(git -C "$PROJECT_ROOT" log --oneline -10 2>/dev/null | grep -v "^[a-z]*(\w*): " | wc -l)
    if [[ $bad_commits -gt 5 ]]; then
        echo -e "${YELLOW}⚠ WARNING: Commit messages don't follow conventional format${NC}"
    else
        echo -e "${GREEN}✓ Commit message format OK${NC}"
    fi
}

# Function to generate compliance report
generate_report() {
    echo ""
    echo "====================================="
    if [[ $FAILED -eq 0 ]]; then
        echo -e "${GREEN}✓ FRAMEWORK COMPLIANCE: PASSED${NC}"
        echo -e "${GREEN}All CISSP-aligned checks passed${NC}"
        exit 0
    else
        echo -e "${RED}✗ FRAMEWORK COMPLIANCE: FAILED${NC}"
        echo -e "${RED}Review violations above${NC}"
        exit 1
    fi
}

# Main execution
echo "Project Root: $PROJECT_ROOT"
echo ""

check_docs_exist
check_docs_freshness
check_security_requirements
check_code_structure
check_change_documentation
generate_report
