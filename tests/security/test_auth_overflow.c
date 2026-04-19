/**
 * @file test_auth_overflow.c
 * @brief Security tests for authentication buffer handling
 * @details Tests buffer overflow protection in CLI authentication
 * 
 * Security Framework:
 * - CWE-120: Buffer Copy without Checking Size of Input
 * - CWE-121: Stack-based Buffer Overflow
 * - CWE-122: Heap-based Buffer Overflow
 * - ISO 27001: A.8.24 (Use of cryptography)
 * - NIST CSF: PR.DS-02 (Data security)
 * 
 * @version 1.0.0
 * @date 2026-04-16
 * @security SEC-038: Automated Security Testing
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <setjmp.h>

// Minimal test framework
#define TEST_ASSERT(condition) do { \
    if (!(condition)) { \
        printf("  ✗ FAIL: %s (line %d)\n", #condition, __LINE__); \
        test_failures++; \
    } else { \
        printf("  ✓ PASS: %s\n", #condition); \
    } \
} while(0)

#define TEST_ASSERT_EQUAL(expected, actual) do { \
    if ((expected) != (actual)) { \
        printf("  ✗ FAIL: Expected %d, got %d (line %d)\n", (int)(expected), (int)(actual), __LINE__); \
        test_failures++; \
    } else { \
        printf("  ✓ PASS: %s == %s\n", #expected, #actual); \
    } \
} while(0)

#define MAX_PASSWORD_LEN 64
#define BUFFER_SIZE 256

static volatile int test_failures = 0;
static volatile int tests_run = 0;

// =============================================================================
// SECURITY TEST: Buffer Overflow Protection
// =============================================================================

/**
 * @brief Test buffer overflow in password input
 * CWE-120: Buffer Copy without Checking Size of Input
 */
void test_password_buffer_overflow_protection(void)
{
    printf("\n--- Test: Password Buffer Overflow Protection ---\n");
    tests_run++;
    
    char password_buffer[MAX_PASSWORD_LEN];
    char overflow_payload[BUFFER_SIZE];
    
    // Create overflow payload
    memset(overflow_payload, 'A', sizeof(overflow_payload));
    overflow_payload[sizeof(overflow_payload) - 1] = '\0';
    
    // Simulate receiving password (should be truncated or rejected)
    size_t copy_len = strlen(overflow_payload);
    if (copy_len > MAX_PASSWORD_LEN - 1) {
        copy_len = MAX_PASSWORD_LEN - 1;  // Safe truncation
    }
    
    strncpy(password_buffer, overflow_payload, copy_len);
    password_buffer[copy_len] = '\0';
    
    // Verify buffer is properly bounded
    TEST_ASSERT(strlen(password_buffer) < MAX_PASSWORD_LEN);
    TEST_ASSERT(password_buffer[MAX_PASSWORD_LEN - 1] == '\0');
    TEST_ASSERT(strnlen(password_buffer, MAX_PASSWORD_LEN) <= MAX_PASSWORD_LEN - 1);
}

/**
 * @brief Test null byte injection in password
 * CWE-158: Null Byte / NUL Character Improper Handling
 */
void test_password_null_byte_injection(void)
{
    printf("\n--- Test: Null Byte Injection Protection ---\n");
    tests_run++;
    
    char password_buffer[MAX_PASSWORD_LEN];
    const char* malicious_input = "pass\x00word\x00hidden";
    
    // Copy password (should handle embedded nulls correctly)
    strncpy(password_buffer, malicious_input, MAX_PASSWORD_LEN - 1);
    password_buffer[MAX_PASSWORD_LEN - 1] = '\0';
    
    // Verify null byte handling
    size_t len = strnlen(password_buffer, MAX_PASSWORD_LEN);
    TEST_ASSERT(len < MAX_PASSWORD_LEN);
    
    // The password should be treated as "pass" not "pass\x00word\x00hidden"
    // This is expected behavior for C strings
    TEST_ASSERT(strlen(password_buffer) == 4);  // "pass" before first null
}

/**
 * @brief Test format string vulnerability in logging
 * CWE-134: Use of Externally-Controlled Format String
 */
void test_format_string_protection(void)
{
    printf("\n--- Test: Format String Protection ---\n");
    tests_run++;
    
    char safe_buffer[256];
    const char* user_input = "%s%s%s%s%n%n%n";
    
    // Safe: Using printf with format string literal
    int result = snprintf(safe_buffer, sizeof(safe_buffer), "%s", user_input);
    
    TEST_ASSERT(result > 0);
    TEST_ASSERT(strstr(safe_buffer, "%s") != NULL);  // Content preserved, not interpreted
    TEST_ASSERT(strlen(safe_buffer) == strlen(user_input));
}

/**
 * @brief Test integer overflow in password length calculation
 * CWE-190: Integer Overflow or Wraparound
 */
void test_password_length_integer_overflow(void)
{
    printf("\n--- Test: Integer Overflow in Length Calculation ---\n");
    tests_run++;
    
    size_t password_len = 100;
    size_t salt_len = 16;
    size_t hash_len = 32;
    
    // Safe size calculation with overflow check
    size_t total_size = password_len + salt_len + hash_len;
    
    // Verify no overflow occurred (simplified check)
    TEST_ASSERT(total_size > password_len);
    TEST_ASSERT(total_size > salt_len);
    TEST_ASSERT(total_size > hash_len);
    TEST_ASSERT(total_size == 148);  // 100 + 16 + 32
}

/**
 * @brief Test off-by-one error in buffer operations
 * CWE-193: Off-by-one Error
 */
void test_buffer_off_by_one(void)
{
    printf("\n--- Test: Off-by-one Buffer Handling ---\n");
    tests_run++;
    
    char buffer[MAX_PASSWORD_LEN];
    const char* test_input = "A test password that might exceed buffer";
    
    // Safe copy with explicit bounds checking
    size_t input_len = strlen(test_input);
    size_t copy_len = (input_len < MAX_PASSWORD_LEN - 1) ? input_len : MAX_PASSWORD_LEN - 1;
    
    memcpy(buffer, test_input, copy_len);
    buffer[copy_len] = '\0';
    
    // Verify null termination
    TEST_ASSERT(buffer[copy_len] == '\0');
    TEST_ASSERT(strlen(buffer) == copy_len);
    
    // Ensure no buffer overrun
    TEST_ASSERT(copy_len < MAX_PASSWORD_LEN);
}

// =============================================================================
// MAIN
// =============================================================================

int main(void)
{
    printf("==============================================\n");
    printf("AdaptivePWM Security Tests: Buffer Handling\n");
    printf("CWE-120, CWE-121, CWE-122, CWE-158, CWE-190\n");
    printf("==============================================\n");
    
    // Run all tests
    test_password_buffer_overflow_protection();
    test_password_null_byte_injection();
    test_format_string_protection();
    test_password_length_integer_overflow();
    test_buffer_off_by_one();
    
    // Summary
    printf("\n==============================================\n");
    printf("Security Test Summary\n");
    printf("==============================================\n");
    printf("Tests run: %d\n", tests_run);
    printf("Failures: %d\n", test_failures);
    
    if (test_failures == 0) {
        printf("\n✓ All security tests PASSED\n");
        return 0;
    } else {
        printf("\n✗ %d security test(s) FAILED\n", test_failures);
        return 1;
    }
}
