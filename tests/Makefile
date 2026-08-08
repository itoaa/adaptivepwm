# Makefile for AdaptivePWM Unit Tests (PWM-ARCH-008)
# Builds and runs all unit tests using Unity framework

# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -g -O0 \
         -I../App/Inc \
         -I../App/Inc/safety \
         -I../Core/Inc \
         -I../config \
         -I. \
         -DUNITY_INCLUDE_CONFIG_H \
         -DDEBUG \
         -DRUNTIME_SAFETY_CHECKS

LDFLAGS = -lm

# Unity framework (optional — place under tests/unity if used)
UNITY_DIR = unity
UNITY_SRC = $(UNITY_DIR)/unity.c

# Test files
TEST_SRCS = \
    test_hal_adc.c \
    test_hal_pwm.c \
    test_hal_uart.c \
    test_pid.c \
    test_param_calc.c \
    test_enhanced_safety.c \
    test_fault_history.c

# Object files
TEST_OBJS = $(TEST_SRCS:.c=.o)
UNITY_OBJ = $(UNITY_SRC:.c=.o)

# Test executables
TEST_BINS = $(TEST_SRCS:.c=)

# Mock and stub sources
MOCK_SRCS = mock_hal.c
MOCK_OBJS = $(MOCK_SRCS:.c=.o)

# Default target: build all tests
.PHONY: all clean test run-tests summary

all: $(TEST_BINS)

# Pattern rule for test executables
%: %.c $(UNITY_OBJ) $(MOCK_OBJS)
	@echo "Building $@..."
	$(CC) $(CFLAGS) -o $@ $< $(UNITY_OBJ) $(MOCK_OBJS) $(LDFLAGS)

# Unity object
$(UNITY_OBJ): $(UNITY_SRC)
	@echo "Building Unity framework..."
	$(CC) $(CFLAGS) -c $< -o $@

# Mock objects
$(MOCK_OBJS): $(MOCK_SRCS)
	@echo "Building mocks..."
	$(CC) $(CFLAGS) -c $< -o $@

# Run all tests
run-tests: $(TEST_BINS)
	@echo "========================================"
	@echo "Running AdaptivePWM Unit Tests"
	@echo "========================================"
	@FAILED=0; \
	PASSED=0; \
	TOTAL=0; \
	for test in $(TEST_BINS); do \
		echo ""; \
		echo "Running $$test..."; \
		./$$test > $$test.log 2>&1; \
		RESULT=$$?; \
		TOTAL=$$((TOTAL + 1)); \
		if [ $$RESULT -eq 0 ]; then \
			echo "  ✓ $$test PASSED"; \
			PASSED=$$((PASSED + 1)); \
		else \
			echo "  ✗ $$test FAILED (exit code $$RESULT)"; \
			FAILED=$$((FAILED + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "========================================"; \
	echo "Test Summary: $$PASSED/$$TOTAL passed"; \
	echo "========================================"; \
	if [ $$FAILED -gt 0 ]; then \
		echo "Failed tests:"; \
		for test in $(TEST_BINS); do \
			if [ ! $$? -eq 0 ] 2>/dev/null; then echo "  - $$test"; fi; \
		done; \
		exit 1; \
	fi

# Summary report
summary:
	@echo "========================================"
	@echo "Unit Test Coverage (PWM-ARCH-008)"
	@echo "========================================"
	@echo ""
	@echo "Test Files:"
	@echo "  ✓ test_hal_adc.c         - HAL ADC module tests"
	@echo "  ✓ test_hal_pwm.c         - HAL PWM module tests"
	@echo "  ✓ test_hal_uart.c        - HAL UART module tests"
	@echo "  ✓ test_pid.c             - PID controller tests"
	@echo "  ✓ test_param_calc.c      - Parameter calculation tests"
	@echo "  ✓ test_enhanced_safety.c - Enhanced safety tests"
	@echo "  ✓ test_fault_history.c   - Fault history tests"
	@echo ""
	@echo "Total: 7 test suites"
	@echo ""
	@echo "Test Coverage Areas:"
	@echo "  - Initialization"
	@echo "  - Normal operations"
	@echo "  - Boundary conditions"
	@echo "  - Error handling"
	@echo "  - Null pointer checks"
	@echo "  - State machine transitions"
	@echo "  - Circular buffer operations"
	@echo "  - Flash operations"

# Individual test targets
test-hal: test_hal_adc test_hal_pwm test_hal_uart
	@echo "Running HAL tests..."
	./test_hal_adc
	./test_hal_pwm
	./test_hal_uart

test-control: test_pid test_param_calc
	@echo "Running control tests..."
	./test_pid
	./test_param_calc

test-safety: test_enhanced_safety test_fault_history
	@echo "Running safety tests..."
	./test_enhanced_safety
	./test_fault_history

# Clean build artifacts
clean:
	rm -f $(TEST_BINS) $(TEST_OBJS) $(UNITY_OBJ) $(MOCK_OBJS)
	rm -f *.log
	rm -f *.o

# Check Unity exists
check-unity:
	@if [ ! -f "$(UNITY_SRC)" ]; then \
		echo "ERROR: Unity framework not found at $(UNITY_SRC)"; \
		echo "Please install Unity testing framework:"; \
		echo "  git clone https://github.com/ThrowTheSwitch/Unity.git $(UNITY_DIR)"; \
		exit 1; \
	fi

# Print help
help:
	@echo "AdaptivePWM Unit Test Makefile (PWM-ARCH-008)"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build all test executables"
	@echo "  run-tests    - Build and run all tests"
	@echo "  test-hal     - Run HAL module tests (ADC, PWM, UART)"
	@echo "  test-control - Run control tests (PID, param_calc)"
	@echo "  test-safety  - Run safety tests (enhanced_safety, fault_history)"
	@echo "  summary      - Display test coverage summary"
	@echo "  clean        - Remove build artifacts"
	@echo "  help         - Show this help"

# Create test summary
test-report: run-tests
	@echo "Generating test report..."
	@echo "AdaptivePWM Unit Test Report" > test-report.txt
	@echo "Generated: $$(date)" >> test-report.txt
	@echo "========================================" >> test-report.txt
	@echo "" >> test-report.txt
	@for test in $(TEST_BINS); do \
		echo "--- $$test ---" >> test-report.txt; \
		cat $$test.log >> test-report.txt 2>/dev/null || true; \
		echo "" >> test-report.txt; \
	done
	@echo "Report saved to: test-report.txt"
