/**
 * @file test_config_parsing.c
 * @brief Security tests for configuration file parsing safety
 * @details Tests for unsafe parsing patterns, buffer handling, and injection attacks
 * 
 * Security Framework:
 * - CWE-676: Use of Potentially Dangerous Function
 * - CWE-20: Improper Input Validation
 * - CWE-119: Improper Restriction of Operations within the Bounds of a Memory Buffer
 * - ISO 27001: A.8.26 (Application security requirements)
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
#include <ctype.h>

// Minimal test framework
#define TEST_ASSERT(condition) do { \
    if (!(condition)) { \
        printf("  ✗ FAIL: %s (line %d)\n", #condition, __LINE__); \
        test_failures++; \
    } else { \
        printf("  ✓ PASS: %s\n", #condition); \
    } \
} while(0)

#define MAX_KEY_LEN 64
#define MAX_VALUE_LEN 256
#define MAX_LINE_LEN 512

static volatile int test_failures = 0;
static volatile int tests_run = 0;

// =============================================================================
// SAFE CONFIG PARSING FUNCTIONS
// =============================================================================

/**
 * @brief Safely parse a configuration line
 * CWE-676: Use of Potentially Dangerous Function
 */
bool safe_parse_config_line(const char* line, char* key, size_t key_size,
                            char* value, size_t value_size) {
    if (!line || !key || !value || key_size == 0 || value_size == 0) {
        return false;
    }
    
    // Skip whitespace
    while (isspace((unsigned char)*line)) line++;
    
    // Skip empty lines and comments
    if (*line == '\0' || *line == '#' || *line == ';') {
        return false;
    }
    
    // Find equals sign
    const char* equals = strchr(line, '=');
    if (!equals) {
        return false;  // No key=value format
    }
    
    // Extract key
    size_t key_len = equals - line;
    if (key_len >= key_size) {
        key_len = key_size - 1;  // Truncate
    }
    
    // Copy and trim key
    memcpy(key, line, key_len);
    key[key_len] = '\0';
    
    // Trim trailing whitespace from key
    while (key_len > 0 && isspace((unsigned char)key[key_len - 1])) {
        key[--key_len] = '\0';
    }
    
    // Extract value
    const char* val_start = equals + 1;
    size_t value_len = strlen(val_start);
    if (value_len >= value_size) {
        value_len = value_size - 1;
    }
    
    // Copy value
    memcpy(value, val_start, value_len);
    value[value_len] = '\0';
    
    // Trim trailing whitespace and newline from value
    while (value_len > 0 && (isspace((unsigned char)value[value_len - 1]) || 
                               value[value_len - 1] == '\n' ||
                               value[value_len - 1] == '\r')) {
        value[--value_len] = '\0';
    }
    
    return key_len > 0 && value_len > 0;
}

/**
 * @brief Validate configuration key
 * CWE-20: Improper Input Validation
 */
bool is_valid_config_key(const char* key) {
    if (!key || strlen(key) == 0 || strlen(key) > MAX_KEY_LEN) {
        return false;
    }
    
    for (size_t i = 0; i < strlen(key); i++) {
        char c = key[i];
        if (!isalnum((unsigned char)c) && c != '_' && c != '-') {
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Validate configuration value
 * CWE-20: Improper Input Validation
 */
bool is_valid_config_value(const char* value) {
    if (!value) return false;
    
    // Check for control characters
    for (size_t i = 0; i < strlen(value); i++) {
        if (!isprint((unsigned char)value[i]) && 
            value[i] != '\n' && value[i] != '\r' && value[i] != '\t') {
            return false;
        }
    }
    
    return strlen(value) < MAX_VALUE_LEN;
}

/**
 * @brief Check for configuration injection attempts
 * CWE-78: OS Command Injection via config
 */
bool contains_config_injection(const char* value) {
    if (!value) return false;
    
    // Check for shell metacharacters
    const char* dangerous = ";|\u0026$`\"\n\r\x00\x3C\x3E";  // < >
test_failures++;
    } else {
        printf("    ✓ PASS: Malicious config value detected: %.30s...\n", malicious_values[i]);
    }
}
}

/**
 * @brief Test key validation
 */
void test_config_key_validation(void)
{
    printf("\n--- Test: Configuration Key Validation ---\n");
    tests_run++;
    
    // Valid keys
    TEST_ASSERT(is_valid_config_key("duty_cycle") == true);
    TEST_ASSERT(is_valid_config_key("pwm_frequency") == true);
    TEST_ASSERT(is_valid_config_key("adc-sample-rate") == true);
    TEST_ASSERT(is_valid_config_key("Kp_gain") == true);
    
    // Invalid keys
    TEST_ASSERT(is_valid_config_key("") == false);
    TEST_ASSERT(is_valid_config_key("key with spaces") == false);
    TEST_ASSERT(is_valid_config_key("key;with;semicolons") == false);
    TEST_ASSERT(is_valid_config_key("key=with=equals") == false);
    TEST_ASSERT(is_valid_config_key("../../../etc") == false);
    TEST_ASSERT(is_valid_config_key("key\nwith\nnewlines") == false);
}

/**
 * @brief Test value validation
 */
void test_config_value_validation(void)
{
    printf("\n--- Test: Configuration Value Validation ---\n");
    tests_run++;
    
    // Valid values
    TEST_ASSERT(is_valid_config_value("50") == true);
    TEST_ASSERT(is_valid_config_value("3.14159") == true);
    TEST_ASSERT(is_valid_config_value("enable") == true);
    TEST_ASSERT(is_valid_config_value("true") == true);
    
    // Invalid values (too long)
    char long_value[MAX_VALUE_LEN + 10];
    memset(long_value, 'A', sizeof(long_value) - 1);
    long_value[sizeof(long_value) - 1] = '\0';
    TEST_ASSERT(is_valid_config_value(long_value) == false);
    
    // Invalid values (control characters)
    TEST_ASSERT(is_valid_config_value("value\x01") == false);
    TEST_ASSERT(is_valid_config_value("value\x1F") == false);
}

/**
 * @brief Test buffer overflow protection in parsing
 */
void test_config_buffer_overflow_protection(void)
{
    printf("\n--- Test: Configuration Buffer Overflow Protection ---\n");
    tests_run++;
    
    char key[16];
    char value[32];
    
    // Test long key truncation
    const char* long_key_line = "very_long_configuration_key_name_that_exceeds_buffer=value";
    bool result = safe_parse_config_line(long_key_line, key, sizeof(key), value, sizeof(value));
    
    if (result) {
        // If parsing succeeded, key should be truncated
        TEST_ASSERT(strlen(key) < sizeof(key));
        TEST_ASSERT(key[sizeof(key) - 1] == '\0');
    }
    
    // Test long value truncation
    char long_value_line[MAX_VALUE_LEN + 100];
    snprintf(long_value_line, sizeof(long_value_line), "key=");
    memset(long_value_line + 4, 'A', MAX_VALUE_LEN + 50);
    long_value_line[MAX_VALUE_LEN + 54] = '\0';
    
    result = safe_parse_config_line(long_value_line, key, sizeof(key), value, sizeof(value));
    if (result) {
        TEST_ASSERT(strlen(value) < sizeof(value));
        TEST_ASSERT(value[sizeof(value) - 1] == '\0');
    }
}

/**
 * @brief Test comment handling
 */
void test_config_comment_handling(void)
{
    printf("\n--- Test: Configuration Comment Handling ---\n");
    tests_run++;
    
    char key[MAX_KEY_LEN];
    char value[MAX_VALUE_LEN];
    
    // Lines starting with # should be ignored
    bool result = safe_parse_config_line("# This is a comment", key, sizeof(key), value, sizeof(value));
    TEST_ASSERT(result == false);
    
    // Lines starting with ; should be ignored
    result = safe_parse_config_line("; This is also a comment", key, sizeof(key), value, sizeof(value));
    TEST_ASSERT(result == false);
    
    // Empty lines should be ignored
    result = safe_parse_config_line("", key, sizeof(key), value, sizeof(value));
    TEST_ASSERT(result == false);
    
    result = safe_parse_config_line("   ", key, sizeof(key), value, sizeof(value));
    TEST_ASSERT(result == false);
}

/**
 * @brief Test missing equals sign
 */
void test_config_missing_equals(void)
{
    printf("\n--- Test: Configuration Missing Equals Sign ---\n");
    tests_run++;
    
    char key[MAX_KEY_LEN];
    char value[MAX_VALUE_LEN];
    
    // Line without equals sign should fail
    bool result = safe_parse_config_line("key_without_equals", key, sizeof(key), value, sizeof(value));
    TEST_ASSERT(result == false);
    
    // Line with only whitespace should fail
    result = safe_parse_config_line("   key   ", key, sizeof(key), value, sizeof(value));
    TEST_ASSERT(result == false);
}

// =============================================================================
// MAIN
// =============================================================================

int main(void)
{
    printf("==============================================\n");
    printf("AdaptivePWM Security Tests: Config Parsing\n");
    printf("CWE-676, CWE-20, CWE-119\n");
    printf("==============================================\n");
    
    // Run all tests
    test_config_line_parsing();
    test_config_injection_detection();
    test_config_key_validation();
    test_config_value_validation();
    test_config_buffer_overflow_protection();
    test_config_comment_handling();
    test_config_missing_equals();
    
    // Summary
    printf("\n==============================================\n");
    printf("Security Test Summary\n");
    printf("==============================================\n");
    printf("Tests run: %d\n", tests_run);
    printf("Failures: %d\n", test_failures);
    
    if (test_failures == 0) {
        printf("\n✓ All configuration parsing tests PASSED\n");
        return 0;
    } else {
        printf("\n✗ %d configuration parsing test(s) FAILED\n", test_failures);
        return 1;
    }
}
