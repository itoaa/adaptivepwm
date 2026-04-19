/**
 * @file test_cli_input.c
 * @brief Security tests for CLI input validation and sanitization
 * @details Tests command injection, path traversal, and input sanitization
 * 
 * Security Framework:
 * - CWE-78: OS Command Injection
 * - CWE-88: Argument Injection or Modification
 * - CWE-20: Improper Input Validation
 * - ISO 27001: A.8.26 (Application security requirements)
 * - NIST CSF: PR.DS-02 (Data security)
 * - CISSP Domain 8: Software Development Security
 * 
 * @version 1.0.0
 * @date 2026-04-16
 * @security SEC-038: Automated Security Testing
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>

// Minimal test framework
#define TEST_ASSERT(condition) do { \
    if (!(condition)) { \
        printf("  ✗ FAIL: %s (line %d)\n", #condition, __LINE__); \
        test_failures++; \
    } else { \
        printf("  ✓ PASS: %s\n", #condition); \
    } \
} while(0)

#define MAX_INPUT_LEN 128
#define MAX_CMD_LEN 32

static volatile int test_failures = 0;
static volatile int tests_run = 0;

// =============================================================================
// INPUT VALIDATION FUNCTIONS (Reference implementations)
// =============================================================================

/**
 * @brief Validate that input contains only printable ASCII
 * CWE-20: Improper Input Validation
 */
bool is_valid_printable(const char* input) {
    if (!input) return false;
    
    for (size_t i = 0; i < strlen(input); i++) {
        if (!isprint((unsigned char)input[i])) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Check for command injection characters
 * CWE-78: OS Command Injection
 */
bool contains_command_chars(const char* input) {
    if (!input) return false;
    
    const char* dangerous = ";|\u0026$`\"\n\r\x00";
    return strpbrk(input, dangerous) != NULL;
}

/**
 * @brief Check for path traversal sequences
 * CWE-22: Improper Limitation of a Pathname to a Restricted Directory
 */
bool contains_path_traversal(const char* input) {
    if (!input) return false;
    
    // Check for ".." sequences
    if (strstr(input, "..") != NULL) {
        return true;
    }
    
    // Check for absolute paths
    if (input[0] == '/' || input[0] == '\\') {
        return true;
    }
    
    // Check for drive letter (Windows)
    if (strlen(input) >= 2 && isalpha((unsigned char)input[0]) && input[1] == ':') {
        return true;
    }
    
    return false;
}

/**
 * @brief Sanitize input by removing control characters
 * CWE-20: Improper Input Validation
 */
void sanitize_input(char* output, const char* input, size_t max_len) {
    if (!output || !input || max_len == 0) return;
    
    size_t j = 0;
    for (size_t i = 0; i < strlen(input) && j < max_len - 1; i++) {
        char c = input[i];
        // Only allow printable ASCII and common whitespace
        if (isprint((unsigned char)c) || c == ' ' || c == '\t') {
            output[j++] = c;
        }
    }
    output[j] = '\0';
}

/**
 * @brief Validate numeric input
 * CWE-20: Improper Input Validation
 */
bool is_valid_number(const char* input) {
    if (!input || strlen(input) == 0) return false;
    
    char* endptr;
    strtod(input, &endptr);
    
    // Check if entire string was consumed
    return *endptr == '\0';
}

/**
 * @brief Check for buffer overflow attempt in numeric input
 * CWE-20: Improper Input Validation
 */
bool is_numeric_overflow_attempt(const char* input) {
    if (!input) return false;
    
    // Check for extremely long numbers
    if (strlen(input) > 20) {
        return true;
    }
    
    // Check for multiple decimal points
    int dot_count = 0;
    for (size_t i = 0; i < strlen(input); i++) {
        if (input[i] == '.') {
            dot_count++;
        }
    }
    
    return dot_count > 1;
}

// =============================================================================
// SECURITY TESTS: CLI Input Validation
// =============================================================================

/**
 * @brief Test command injection detection
 * CWE-78: OS Command Injection
 */
void test_command_injection_detection(void)
{
    printf("\n--- Test: Command Injection Detection ---\n");
    tests_run++;
    
    // These should be detected as dangerous
    TEST_ASSERT(contains_command_chars("; rm -rf /") == true);
    TEST_ASSERT(contains_command_chars("| cat /etc/passwd") == true);
    TEST_ASSERT(contains_command_chars("\u0026& reboot") == true);
    TEST_ASSERT(contains_command_chars("`whoami`") == true);
    TEST_ASSERT(contains_command_chars("$(id)") == true);
    TEST_ASSERT(contains_command_chars("echo hello; ls") == true);
    TEST_ASSERT(contains_command_chars("test\nreboot") == true);
    TEST_ASSERT(contains_command_chars("test\rmalicious") == true);
    
    // These should be safe
    TEST_ASSERT(contains_command_chars("pwm start") == false);
    TEST_ASSERT(contains_command_chars("config duty 50") == false);
    TEST_ASSERT(contains_command_chars("status") == false);
    TEST_ASSERT(contains_command_chars("monitor 10") == false);
    TEST_ASSERT(contains_command_chars("hello world") == false);
}

/**
 * @brief Test path traversal detection
 * CWE-22: Improper Limitation of a Pathname
 */
void test_path_traversal_detection(void)
{
    printf("\n--- Test: Path Traversal Detection ---\n");
    tests_run++;
    
    // These should be detected as path traversal
    TEST_ASSERT(contains_path_traversal("../../../etc/passwd") == true);
    TEST_ASSERT(contains_path_traversal("..\\..\\..\\windows\\system32") == true);
    TEST_ASSERT(contains_path_traversal("/etc/passwd") == true);
    TEST_ASSERT(contains_path_traversal("C:\\Windows\\System32") == true);
    TEST_ASSERT(contains_path_traversal("/absolute/path") == true);
    TEST_ASSERT(contains_path_traversal("..\\\\secret") == true);
    
    // These should be safe (relative paths without traversal)
    TEST_ASSERT(contains_path_traversal("config.json") == false);
    TEST_ASSERT(contains_path_traversal("logs/error.txt") == false);
    TEST_ASSERT(contains_path_traversal("data/measurements.csv") == false);
    TEST_ASSERT(contains_path_traversal(".hidden") == false);
}

/**
 * @brief Test input sanitization
 * CWE-20: Improper Input Validation
 */
void test_input_sanitization(void)
{
    printf("\n--- Test: Input Sanitization ---\n");
    tests_run++;
    
    char output[MAX_INPUT_LEN];
    
    // Test control character removal
    memset(output, 0, sizeof(output));
    sanitize_input(output, "hello\x01world", MAX_INPUT_LEN);
    TEST_ASSERT(strcmp(output, "helloworld") == 0);
    
    // Test null byte handling
    memset(output, 0, sizeof(output));
    sanitize_input(output, "test\x00password", MAX_INPUT_LEN);
    TEST_ASSERT(strcmp(output, "test") == 0);  // Stops at null
    
    // Test normal input preservation
    memset(output, 0, sizeof(output));
    sanitize_input(output, "pwm duty 50.5", MAX_INPUT_LEN);
    TEST_ASSERT(strcmp(output, "pwm duty 50.5") == 0);
    
    // Test maximum length enforcement
    memset(output, 0, sizeof(output));
    char long_input[300];
    memset(long_input, 'A', sizeof(long_input) - 1);
    long_input[sizeof(long_input) - 1] = '\0';
    sanitize_input(output, long_input, MAX_INPUT_LEN);
    TEST_ASSERT(strlen(output) == MAX_INPUT_LEN - 1);
}

/**
 * @brief Test printable ASCII validation
 * CWE-20: Improper Input Validation
 */
void test_printable_ascii_validation(void)
{
    printf("\n--- Test: Printable ASCII Validation ---\n");
    tests_run++;
    
    // Valid printable ASCII
    TEST_ASSERT(is_valid_printable("Hello World") == true);
    TEST_ASSERT(is_valid_printable("pwm start 50") == true);
    TEST_ASSERT(is_valid_printable("status") == true);
    TEST_ASSERT(is_valid_printable("duty=0.5") == true);
    
    // Invalid - control characters
    TEST_ASSERT(is_valid_printable("\x01test") == false);
    TEST_ASSERT(is_valid_printable("test\x1F") == false);
    TEST_ASSERT(is_valid_printable("\x7F") == false);  // DEL
    TEST_ASSERT(is_valid_printable("\x80") == false);  // Extended ASCII
    
    // Edge cases
    TEST_ASSERT(is_valid_printable("") == true);  // Empty is valid
    TEST_ASSERT(is_valid_printable(NULL) == false);  // NULL is invalid
}

/**
 * @brief Test numeric input validation
 * CWE-20: Improper Input Validation
 */
void test_numeric_input_validation(void)
{
    printf("\n--- Test: Numeric Input Validation ---\n");
    tests_run++;
    
    // Valid numbers
    TEST_ASSERT(is_valid_number("50") == true);
    TEST_ASSERT(is_valid_number("50.5") == true);
    TEST_ASSERT(is_valid_number("-10") == true);
    TEST_ASSERT(is_valid_number("0.001") == true);
    TEST_ASSERT(is_valid_number("1000000") == true);
    
    // Invalid numbers
    TEST_ASSERT(is_valid_number("50.5.5") == false);  // Multiple decimals
    TEST_ASSERT(is_valid_number("abc") == false);
    TEST_ASSERT(is_valid_number("50abc") == false);
    TEST_ASSERT(is_valid_number("") == false);
    TEST_ASSERT(is_valid_number(" ") == false);
    TEST_ASSERT(is_valid_number("--50") == false);
}

/**
 * @brief Test numeric overflow attempt detection
 * CWE-20: Improper Input Validation
 */
void test_numeric_overflow_detection(void)
{
    printf("\n--- Test: Numeric Overflow Detection ---\n");
    tests_run++;
    
    // Overflow attempts
    char big_number[30];
    memset(big_number, '9', sizeof(big_number) - 1);
    big_number[sizeof(big_number) - 1] = '\0';
    TEST_ASSERT(is_numeric_overflow_attempt(big_number) == true);
    
    // Multiple decimal points
    TEST_ASSERT(is_numeric_overflow_attempt("50.5.5.5") == true);
    TEST_ASSERT(is_numeric_overflow_attempt("1.2.3") == true);
    
    // Normal numbers (should not be flagged)
    TEST_ASSERT(is_numeric_overflow_attempt("50") == false);
    TEST_ASSERT(is_numeric_overflow_attempt("50.5") == false);
    TEST_ASSERT(is_numeric_overflow_attempt("12345678901234567890") == false);  // Still under 20 chars
}

/**
 * @brief Test combined validation for typical CLI commands
 */
void test_cli_command_validation(void)
{
    printf("\n--- Test: CLI Command Validation ---\n");
    tests_run++;
    
    // Valid commands should pass all checks
    const char* valid_commands[] = {
        "status",
        "pwm start",
        "pwm stop",
        "pwm duty 50",
        "monitor 10",
        "config duty 0.5",
        "errors clear",
        "help"
    };
    
    for (size_t i = 0; i < sizeof(valid_commands) / sizeof(valid_commands[0]); i++) {
        bool is_valid = !contains_command_chars(valid_commands[i]) &&
                       !contains_path_traversal(valid_commands[i]) &&
                       is_valid_printable(valid_commands[i]);
        if (!is_valid) {
            printf("    ✗ FAIL: Valid command flagged as invalid: %s\n", valid_commands[i]);
            test_failures++;
        } else {
            printf("    ✓ PASS: Valid command accepted: %s\n", valid_commands[i]);
        }
    }
    
    // Malicious commands should be detected
    const char* malicious_commands[] = {
        "status; rm -rf /",
        "config | cat /etc/passwd",
        "monitor ../../../etc/passwd",
        "pwm `reboot`",
        "duty $(whoami)"
    };
    
    for (size_t i = 0; i < sizeof(malicious_commands) / sizeof(malicious_commands[0]); i++) {
        bool is_dangerous = contains_command_chars(malicious_commands[i]) ||
                           contains_path_traversal(malicious_commands[i]);
        if (!is_dangerous) {
            printf("    ✗ FAIL: Malicious command not detected: %s\n", malicious_commands[i]);
            test_failures++;
        } else {
            printf("    ✓ PASS: Malicious command detected: %s\n", malicious_commands[i]);
        }
    }
}

// =============================================================================
// MAIN
// =============================================================================

int main(void)
{
    printf("==============================================\n");
    printf("AdaptivePWM Security Tests: CLI Input Validation\n");
    printf("CWE-20, CWE-78, CWE-88, CWE-22\n");
    printf("==============================================\n");
    
    // Run all tests
    test_command_injection_detection();
    test_path_traversal_detection();
    test_input_sanitization();
    test_printable_ascii_validation();
    test_numeric_input_validation();
    test_numeric_overflow_detection();
    test_cli_command_validation();
    
    // Summary
    printf("\n==============================================\n");
    printf("Security Test Summary\n");
    printf("==============================================\n");
    printf("Tests run: %d\n", tests_run);
    printf("Failures: %d\n", test_failures);
    
    if (test_failures == 0) {
        printf("\n✓ All CLI input validation tests PASSED\n");
        return 0;
    } else {
        printf("\n✗ %d CLI input validation test(s) FAILED\n", test_failures);
        return 1;
    }
}
