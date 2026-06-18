#!/bin/bash

# Test script for toolchain verification
# This script tests that the verification script works correctly

set -euo pipefail

echo "Testing toolchain verification script..."

# Test 1: Check that the script exists and is executable
if [[ -f "./tools/verify-toolchain.sh" && -x "./tools/verify-toolchain.sh" ]]; then
    echo "✓ Verification script exists and is executable"
else
    echo "✗ Verification script missing or not executable"
    exit 1
fi

# Test 2: Check that the script runs without errors
if ./tools/verify-toolchain.sh --help >/dev/null 2>&1; then
    echo "✓ Verification script runs and accepts --help flag"
else
    echo "✗ Verification script failed to run with --help"
    exit 1
fi

# Test 3: Run a basic verification check
if ./tools/verify-toolchain.sh >/dev/null 2>&1; then
    echo "✓ Verification script runs successfully"
else
    echo "✗ Verification script failed to run"
    exit 1
fi

echo "All tests passed!"