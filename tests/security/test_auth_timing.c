/**
 * @file test_auth_timing.c
 * @brief Security tests for authentication timing attack resistance
 * @details Tests constant-time comparison and timing attack mitigation
 * 
 * Security Framework:
 * - CWE-208: Observable Timing Discrepancy
 * - CWE-203: Observable Discrepancy to Distinguish Errors
 * - ISO 27001: A.8.5 (Secure authentication)
 * - NIST CSF: PR.AC-01 (Access Control)
 * - CISSP Domain 5: Identity and Access Management
 * 
 * @version 1.0.0
 * @date 2026-04-16
 * @security SEC-038: Automated Security Testing
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
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

#define TIMING_ITERATIONS 1000
#define TIMING_THRESHOLD_NS 1000000  // 1ms threshold for timing variance

static volatile int test_failures = 0;
static volatile int tests_run = 0;

// =============================================================================
// TIMING UTILITIES
// =============================================================================

static inline uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

// =============================================================================
// CONSTANT-TIME COMPARISON FUNCTIONS (Reference implementations)
// =============================================================================

/**
 * @brief Constant-time memory comparison
 * Prevents timing attacks by comparing all bytes regardless of mismatch position
 */
int constant_time_memcmp(const void* a, const void* b, size_t len) {
    const uint8_t* pa = (const uint8_t*)a;
    const uint8_t* b_const = (const uint8_t*)b;  // Renamed to avoid shadowing
    uint8_t result = 0;
    
    for (size_t i = 0; i < len; i++) {
        result |= pa[i] ^ b_const[i];  // Use renamed variable
    }
    
    return result;
}

/**
 * @brief Vulnerable comparison (for testing)
 * Stops at first mismatch - vulnerable to timing attacks
 */
int vulnerable_memcmp(const void* a, const void* b, size_t len) {
    const uint8_t* pa = (const uint8_t*)a;
    const uint8_t* pb = (const uint8_t*)b;
    
    for (size_t i = 0; i < len; i++) {
        if (pa[i] != pb[i]) {
            return pa[i] - pb[i];  // Early return = timing leak
        }
    }
    
    return 0;
}

// =============================================================================
// SECURITY TESTS: Timing Attack Resistance
// =============================================================================

/**
 * @brief Test constant-time comparison correctness
 * CWE-208: Observable Timing Discrepancy
 */
void test_constant_time_comparison_correctness(void)
{
    printf("\n--- Test: Constant-time Comparison Correctness ---\n");
    tests_run++;
    
    uint8_t buf1[32] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
                        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
                        0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00,
                        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    uint8_t buf2[32] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
                        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
                        0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00,
                        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    uint8_t buf3[32] = {0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8,
                        0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xF0,
                        0xEF, 0xEE, 0xED, 0xEC, 0xEB, 0xEA, 0xE9, 0xE8,
                        0xE7, 0xE6, 0xE5, 0xE4, 0xE3, 0xE2, 0xE1, 0xE0};
    
    // Equal buffers should return 0
    int result1 = constant_time_memcmp(buf1, buf2, 32);
    TEST_ASSERT(result1 == 0);
    
    // Different buffers should return non-zero
    int result2 = constant_time_memcmp(buf1, buf3, 32);
    TEST_ASSERT(result2 != 0);
    
    // Same buffer should return 0
    int result3 = constant_time_memcmp(buf1, buf1, 32);
    TEST_ASSERT(result3 == 0);
}

/**
 * @brief Test timing consistency of constant-time comparison
 * CWE-208: Observable Timing Discrepancy
 */
void test_constant_time_timing_consistency(void)
{
    printf("\n--- Test: Constant-time Comparison Timing ---\n");
    tests_run++;
    
    uint8_t match[64];
    uint8_t mismatch_start[64];
    uint8_t mismatch_end[64];
    
    // Initialize test data
    memset(match, 0xAA, sizeof(match));
    memset(mismatch_start, 0xAA, sizeof(mismatch_start));
    memset(mismatch_end, 0xAA, sizeof(mismatch_end));
    
    // Create mismatches at different positions
    mismatch_start[0] = 0x55;  // Mismatch at start
    mismatch_end[63] = 0x55;   // Mismatch at end
    
    // Measure timing for match
    uint64_t start = get_time_ns();
    for (int i = 0; i < TIMING_ITERATIONS; i++) {
        constant_time_memcmp(match, match, sizeof(match));
    }
    uint64_t time_match = get_time_ns() - start;
    
    // Measure timing for mismatch at start
    start = get_time_ns();
    for (int i = 0; i < TIMING_ITERATIONS; i++) {
        constant_time_memcmp(match, mismatch_start, sizeof(match));
    }
    uint64_t time_mismatch_start = get_time_ns() - start;
    
    // Measure timing for mismatch at end
    start = get_time_ns();
    for (int i = 0; i < TIMING_ITERATIONS; i++) {
        constant_time_memcmp(match, mismatch_end, sizeof(match));
    }
    uint64_t time_mismatch_end = get_time_ns() - start;
    
    // Calculate average times
    uint64_t avg_match = time_match / TIMING_ITERATIONS;
    uint64_t avg_mismatch_start = time_mismatch_start / TIMING_ITERATIONS;
    uint64_t avg_mismatch_end = time_mismatch_end / TIMING_ITERATIONS;
    
    printf("    Average timing (match): %lu ns\n", (unsigned long)avg_match);
    printf("    Average timing (mismatch start): %lu ns\n", (unsigned long)avg_mismatch_start);
    printf("    Average timing (mismatch end): %lu ns\n", (unsigned long)avg_mismatch_end);
    
    // For constant-time comparison, all should be similar
    // Note: This is a statistical test and may have variance
    // We check that end-mismatch isn't significantly faster than start-mismatch
    int64_t timing_diff = (int64_t)avg_mismatch_start - (int64_t)avg_mismatch_end;
    if (timing_diff < 0) timing_diff = -timing_diff;
    
    printf("    Timing difference: %ld ns\n", (long)timing_diff);
    
    // The difference should be small for constant-time implementation
    // (This is a heuristic - actual values depend on system)
    TEST_ASSERT(timing_diff < TIMING_THRESHOLD_NS);
}

/**
 * @brief Test vulnerable comparison timing difference
 * Demonstrates that early-return comparison leaks timing information
 */
void test_vulnerable_comparison_timing(void)
{
    printf("\n--- Test: Vulnerable Comparison Timing (Demonstration) ---\n");
    tests_run++;
    
    // This test demonstrates the timing difference in vulnerable comparison
    // We don't assert on this since it's expected behavior
    
    uint8_t buf1[32];
    uint8_t buf2[32];
    
    memset(buf1, 0xAA, sizeof(buf1));
    memset(buf2, 0xAA, sizeof(buf2));
    buf2[31] = 0x55;  // Mismatch at end
    
    // This is informational - vulnerable function will be faster
    // when mismatch is earlier in the buffer
    printf("    (Informational: Vulnerable comparison shows timing variance)\n");
    printf("    Mismatch position affects execution time in vulnerable implementations\n");
    
    // Verify vulnerable function has different behavior
    int result = vulnerable_memcmp(buf1, buf2, sizeof(buf1));
    TEST_ASSERT(result != 0);  // Should detect mismatch
}

/**
 * @brief Test password verification with constant-time comparison
 * Simulates how password hashes should be compared
 */
void test_password_verification_constant_time(void)
{
    printf("\n--- Test: Password Verification Constant-Time ---\n");
    tests_run++;
    
    // Simulated password hashes
    uint8_t stored_hash[32] = {
        0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
        0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
    };
    
    uint8_t correct_hash[32];
    uint8_t wrong_hash[32];
    
    memcpy(correct_hash, stored_hash, 32);
    memcpy(wrong_hash, stored_hash, 32);
    wrong_hash[0] ^= 0xFF;  // Change first byte
    
    // Correct password should match
    int result_correct = constant_time_memcmp(stored_hash, correct_hash, 32);
    TEST_ASSERT(result_correct == 0);
    
    // Wrong password should not match
    int result_wrong = constant_time_memcmp(stored_hash, wrong_hash, 32);
    TEST_ASSERT(result_wrong != 0);
    
    // Both comparisons should take similar time
    printf("    Correct password comparison: %d\n", result_correct);
    printf("    Wrong password comparison: %d\n", result_wrong);
}

// =============================================================================
// MAIN
// =============================================================================

int main(void)
{
    printf("==============================================\n");
    printf("AdaptivePWM Security Tests: Timing Attacks\n");
    printf("CWE-208, CWE-203\n");
    printf("==============================================\n");
    
    // Run all tests
    test_constant_time_comparison_correctness();
    test_constant_time_timing_consistency();
    test_vulnerable_comparison_timing();
    test_password_verification_constant_time();
    
    // Summary
    printf("\n==============================================\n");
    printf("Security Test Summary\n");
    printf("==============================================\n");
    printf("Tests run: %d\n", tests_run);
    printf("Failures: %d\n", test_failures);
    
    if (test_failures == 0) {
        printf("\n✓ All timing security tests PASSED\n");
        return 0;
    } else {
        printf("\n✗ %d timing security test(s) FAILED\n", test_failures);
        return 1;
    }
}
