.PHONY: all clean build flash monitor test docs help test-unit test-unit-build test-unit-run test-unit-help

# Default target
all: build

# Build release version
build:
	platformio run -e nucleo_f401re

# Build debug version
build-debug:
	platformio run -e nucleo_f401re_debug

# Upload firmware
flash:
	platformio run --target upload

# Clean build artifacts
clean:
	platformio run --target clean
	-rm -rf .pio/build

# Monitor serial output
monitor:
	platformio device monitor -b 115200

# Run tests (legacy Python tests)
test:
	cd test && python3 -m pytest test_adaptivepwm.py -v

# =============================================================================
# UNIT TEST TARGETS - Added for PWM-ARCH-008
# =============================================================================

# Build all unit tests
test-unit-build:
	@echo "========================================"
	@echo "Building Unit Tests (PWM-ARCH-008)"
	@echo "========================================"
	cd tests && $(MAKE) all

# Run all unit tests
test-unit-run:
	@echo "========================================"
	@echo "Running Unit Tests (PWM-ARCH-008)"
	@echo "========================================"
	cd tests && $(MAKE) run-tests

# Build and run unit tests
test-unit: test-unit-build test-unit-run
	@echo "Unit tests complete"

# Show unit test summary
test-unit-summary:
	@cd tests && $(MAKE) summary

# Clean unit test artifacts
test-unit-clean:
	cd tests && $(MAKE) clean

# Show unit test help
test-unit-help:
	@echo "Unit Test Targets (PWM-ARCH-008):"
	@echo "  test-unit-build     - Build all unit test executables"
	@echo "  test-unit-run       - Run all unit tests"
	@echo "  test-unit           - Build and run all unit tests"
	@echo "  test-unit-summary   - Show test coverage summary"
	@echo "  test-unit-clean     - Clean unit test build artifacts"
	@echo ""
	@echo "Test Suites:"
	@echo "  test_hal_adc.c         - HAL ADC module tests"
	@echo "  test_hal_pwm.c         - HAL PWM module tests"
	@echo "  test_hal_uart.c        - HAL UART module tests"
	@echo "  test_pid.c             - PID controller tests"
	@echo "  test_param_calc.c      - Parameter calculation tests"
	@echo "  test_enhanced_safety.c - Enhanced safety tests"
	@echo "  test_fault_history.c   - Fault history tests"

# Generate documentation
docs:
	cd docs && mkdocs serve

# Static analysis
check:
	cppcheck --enable=all src/

# Format code
format:
	clang-format -i src/*.c src/*.h

# Size report
size:
	arm-none-eabi-size .pio/build/*/firmware.elf

# Disassembly
disasm:
	arm-none-eabi-objdump -d .pio/build/*/firmware.elf > firmware.lst

# =============================================================================
# SECURITY TARGETS - Added for SEC-038
# =============================================================================

# Run all security checks
security-check: security-cppcheck security-clang-tidy security-tests
	@echo "========================================"
	@echo "Security Checks Complete"
	@echo "========================================"

# Run cppcheck security scan
security-cppcheck:
	@echo "========================================"
	@echo "Running Cppcheck Security Scan..."
	@echo "========================================"
	@cppcheck \
		--enable=all \
		--addon=security \
		--addon=cert \
		--suppressions-list=ci/cppcheck-suppressions.txt \
		--template='{file}:{line}:{column}: {severity}: {message} [{id}]' \
		--check-level=exhaustive \
		--force \
		--error-exitcode=0 \
		src/ include/ \
		2>&1 | tee security-cppcheck-report.txt || true
	@echo "Report saved to: security-cppcheck-report.txt"

# Run clang-tidy security scan
security-clang-tidy:
	@echo "========================================"
	@echo "Running Clang-Tidy Security Scan..."
	@echo "========================================"
	@find src -name "*.c" -o -name "*.cpp" 2>/dev/null | head -20 | while read file; do \
		echo "Checking: $$file"; \
		clang-tidy \
			"$$file" \
			--config-file=.clang-tidy-security \
			-- \
			-Isrc -Iinclude \
			2>&1 | tee -a security-clang-tidy-report.txt || true; \
	done
	@echo "Report saved to: security-clang-tidy-report.txt"

# Run security-specific tests
security-tests:
	@echo "========================================"
	@echo "Running Security Tests..."
	@echo "========================================"
	@mkdir -p security-test-reports
	@for file in tests/security/*.c; do \
		if [ -f "$$file" ]; then \
			echo "Compiling: $$(basename $$file)"; \
			gcc -o security-test-reports/$$(basename $$file .c) \
				"$$file" \
				-lm \
				2>&1 | tee security-test-reports/build.log || true; \
			if [ -f "security-test-reports/$$(basename $$file .c)" ]; then \
				echo "Running: $$(basename $$file .c)"; \
				./security-test-reports/$$(basename $$file .c) \
					2>&1 | tee security-test-reports/$$(basename $$file .c).log || true; \
			fi; \
		fi; \
	done
	@echo "Security test reports saved to: security-test-reports/"

# Clean security reports
security-clean:
	@echo "Cleaning security reports..."
	@rm -f security-cppcheck-report.txt
	@rm -f security-clang-tidy-report.txt
	@rm -rf security-test-reports/
	@echo "Security reports cleaned"

# Run security scan with detailed output
security-scan-full: security-clean security-cppcheck security-clang-tidy
	@echo "========================================"
	@echo "Full Security Scan Complete"
	@echo "========================================"
	@echo "Reports generated:"
	@echo "  - security-cppcheck-report.txt"
	@echo "  - security-clang-tidy-report.txt"
	@echo "  - security-test-reports/"

# Show security help
security-help:
	@echo "Security targets:"
	@echo "  security-check       - Run all security scans"
	@echo "  security-cppcheck    - Run cppcheck security scan"
	@echo "  security-clang-tidy  - Run clang-tidy security scan"
	@echo "  security-tests       - Run security-specific test suite"
	@echo "  security-clean       - Clean security reports"
	@echo "  security-scan-full   - Complete security scan with fresh reports"

# =============================================================================
# MISRA-C COMPLIANCE TARGETS - Added for PWM-ARCH-010
# =============================================================================

# Run MISRA-C:2012 compliance check
misra-check:
	@echo "========================================"
	@echo "Running MISRA-C:2012 Compliance Check..."
	@echo "========================================"
	@cppcheck \
		--enable=all \
		--addon=misra \
		--suppressions-list=ci/misra-suppressions.txt \
		--template='{file}:{line}:{column}: {severity}: {message} [{id}]' \
		--check-level=exhaustive \
		--force \
		--error-exitcode=0 \
		src/ \
		2>&1 | tee misra-report.txt || true
	@echo "MISRA report saved to: misra-report.txt"
	@cat misra-report.txt | grep -E "(misra|Rule|rule)" | wc -l | xargs echo "Total MISRA issues found:"

# Run MISRA check with detailed rule reporting
misra-report:
	@echo "========================================"
	@echo "Generating MISRA Compliance Report..."
	@echo "========================================"
	@mkdir -p misra-reports
	@cppcheck \
		--enable=all \
		--addon=misra \
		--suppressions-list=ci/misra-suppressions.txt \
		--xml \
		--check-level=exhaustive \
		--force \
		--error-exitcode=0 \
		src/ \
		2> misra-reports/misra-report.xml || true
	@echo "XML report saved to: misra-reports/misra-report.xml"
	@cppcheck \
		--enable=all \
		--addon=misra \
		--suppressions-list=ci/misra-suppressions.txt \
		--template='{file}:{line}: {severity}: {message} [{id}]' \
		--check-level=exhaustive \
		--force \
		--error-exitcode=0 \
		src/ \
		2> misra-reports/misra-violations.txt || true
	@echo "Text report saved to: misra-reports/misra-violations.txt"

# Check specific MISRA rule
misra-rule:
	@echo "Usage: make misra-rule RULE=15.5"
	@echo "Checking MISRA Rule $(RULE)..."
	@cppcheck \
		--enable=all \
		--addon=misra \
		--suppressions-list=ci/misra-suppressions.txt \
		--template='{file}:{line}: {severity}: {message} [{id}]' \
		src/ \
		2>&1 | grep -i "$(RULE)" || echo "No violations of rule $(RULE) found"

# List files with MISRA violations
misra-files:
	@echo "========================================"
	@echo "Files with MISRA Violations:"
	@echo "========================================"
	@cppcheck \
		--enable=all \
		--addon=misra \
		--suppressions-list=ci/misra-suppressions.txt \
		--template='{file}' \
		src/ \
		2>&1 | sort | uniq | grep "\.c$$\|\.h$$" || true

# Clean MISRA reports
misra-clean:
	@echo "Cleaning MISRA reports..."
	@rm -f misra-report.txt
	@rm -rf misra-reports/
	@echo "MISRA reports cleaned"

# Show MISRA help
misra-help:
	@echo "MISRA-C:2012 Compliance Targets:"
	@echo "  misra-check    - Run MISRA compliance check"
	@echo "  misra-report   - Generate detailed MISRA report (XML + text)"
	@echo "  misra-rule     - Check specific rule (use: make misra-rule RULE=15.5)"
	@echo "  misra-files    - List files with MISRA violations"
	@echo "  misra-clean    - Clean MISRA reports"
	@echo ""
	@echo "Documentation:"
	@echo "  ci/misra-config.txt        - MISRA configuration"
	@echo "  ci/misra-suppressions.txt  - Approved suppressions"
	@echo "  docs/MISRA_COMPLIANCE.md   - Full compliance report"

# =============================================================================
# STANDARD TARGETS
# =============================================================================

# Help
help:
	@echo "Available targets:"
	@echo "  build       - Build release version"
	@echo "  build-debug - Build debug version"
	@echo "  flash       - Upload to target"
	@echo "  clean       - Clean build files"
	@echo "  monitor     - Open serial monitor"
	@echo "  test        - Run Python unit tests"
	@echo "  test-unit   - Build and run C unit tests (PWM-ARCH-008)"
	@echo "  docs        - Serve documentation"
	@echo "  check       - Run static analysis"
	@echo "  format      - Format source code"
	@echo "  size        - Show firmware size"
	@echo "  disasm      - Generate disassembly"
	@echo ""
	@echo "Unit test targets (make test-unit-help for more):"
	@echo "  test-unit-build      - Build unit tests"
	@echo "  test-unit-run        - Run unit tests"
	@echo "  test-unit-summary    - Show test summary"
	@echo "  test-unit-clean      - Clean unit test artifacts"
	@echo ""
	@echo "Security targets (make security-help for more):"
	@echo "  security-check       - Run all security scans"
	@echo "  security-cppcheck    - Run cppcheck security scan"
	@echo "  security-clang-tidy  - Run clang-tidy security scan"
	@echo "  security-tests       - Run security-specific test suite"
	@echo ""
	@echo "MISRA targets (make misra-help for more):"
	@echo "  misra-check    - Run MISRA compliance check"
	@echo "  misra-report   - Generate detailed MISRA report"
	@echo "  misra-rule     - Check specific MISRA rule"
	@echo "  misra-files    - List files with violations"
