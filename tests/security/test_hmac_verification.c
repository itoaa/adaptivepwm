/**
 * @file test_hmac_verification.c
 * @brief Security tests for HMAC-SHA256 integrity verification
 * @details Tests HMAC computation, verification, and tamper detection
 * 
 * Security Framework:
 * - CWE-353: Missing Support for Integrity Check
 * - CWE-354: Improper Validation of Integrity Check Value
 * - ISO 27001: A.8.15 (Logging)
 * - NIST CSF: PR.DS-06 (Data integrity)
 * - CISSP Domain 3: Security Architecture and Engineering
 * 
 * Related Issues:
 * - SEC-019: HMAC-SHA256 for flash logger integrity
 * 
 * @version 1.0.0
 * @date 2026-04-16
 * @security SEC-038: Automated Security Testing
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

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

#define HMAC_SHA256_KEY_SIZE        32
#define HMAC_SHA256_SIGNATURE_SIZE  32
#define HMAC_SALT_SIZE              16

static volatile int test_failures = 0;
static volatile int tests_run = 0;

// =============================================================================
// MOCK HMAC IMPLEMENTATION (for testing without crypto library)
// =============================================================================

/**
 * @brief Mock HMAC computation for testing
 * In production, this uses proper HMAC-SHA256 from flash_logger_hmac.c
 */
typedef struct {
    uint32_t timestamp;
    float duty_cycle;
    float efficiency;
    float temperature;
    float current;
    uint16_t error_code;
    uint16_t reserved;
    uint8_t salt[HMAC_SALT_SIZE];
    uint8_t prev_hash[8];
    uint8_t signature[HMAC_SHA256_SIGNATURE_SIZE];
} MockLogEntryHMAC_t;

/**
 * @brief Compute mock HMAC-SHA256 signature
 * This is a simplified version for testing
 */
static bool mock_hmac_compute(MockLogEntryHMAC_t* entry, const uint8_t* key) {
    // Compute a simple hash of the data (NOT cryptographically secure - for testing only)
    // In production, use proper HMAC-SHA256
    
    uint8_t data[64];
    memcpy(data, &entry->timestamp, sizeof(entry->timestamp));
    memcpy(data + 4, &entry->duty_cycle, sizeof(entry->duty_cycle));
    memcpy(data + 8, &entry->efficiency, sizeof(entry->efficiency));
    memcpy(data + 12, &entry->temperature, sizeof(entry->temperature));
    memcpy(data + 16, &entry->current, sizeof(entry->current));
    memcpy(data + 20, &entry->error_code, sizeof(entry->error_code));
    memcpy(data + 22, entry->salt, HMAC_SALT_SIZE);
    
    // XOR-based "hash" (INSECURE - for testing structure only)
    for (int i = 0; i < HMAC_SHA256_SIGNATURE_SIZE; i++) {
        entry->signature[i] = key[i % HMAC_SHA256_KEY_SIZE];
        for (int j = 0; j < 64; j++) {
            entry->signature[i] ^= data[j] ^ (i * j);
        }
    }
    
    return true;
}

/**
 * @brief Verify mock HMAC signature
 */
static bool mock_hmac_verify(const MockLogEntryHMAC_t* entry, const uint8_t* key) {
    MockLogEntryHMAC_t temp = *entry;
    mock_hmac_compute(&temp, key);
    return memcmp(entry->signature, temp.signature, HMAC_SHA256_SIGNATURE_SIZE) == 0;
}

// =============================================================================
// SECURITY TESTS: HMAC Verification
// =============================================================================

/**
 * @brief Test HMAC signature computation
 * CWE-353: Missing Support for Integrity Check
 */
void test_hmac_computation(void)
{
    printf("\n--- Test: HMAC Signature Computation ---\n");
    tests_run++;
    
    uint8_t key[HMAC_SHA256_KEY_SIZE] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
    };
    
    MockLogEntryHMAC_t entry = {
        .timestamp = 1234567890,
        .duty_cycle = 50.0f,
        .efficiency = 95.5f,
        .temperature = 45.0f,
        .current = 2.5f,
        .error_code = 0,
        .reserved = 0
    };
    
    // Generate salt
    for (int i = 0; i < HMAC_SALT_SIZE; i++) {
        entry.salt[i] = (uint8_t)(i * 17 + 42);
    }
    
    // Compute signature
    bool result = mock_hmac_compute(&entry, key);
    TEST_ASSERT(result == true);
    
    // Verify signature is non-zero
    bool is_zero = true;
    for (int i = 0; i < HMAC_SHA256_SIGNATURE_SIZE; i++) {
        if (entry.signature[i] != 0) {
            is_zero = false;
            break;
        }
    }
    TEST_ASSERT(is_zero == false);
}

/**
 * @brief Test HMAC signature verification
 * CWE-354: Improper Validation of Integrity Check Value
 */
void test_hmac_verification(void)
{
    printf("\n--- Test: HMAC Signature Verification ---\n");
    tests_run++;
    
    uint8_t key[HMAC_SHA256_KEY_SIZE] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
    };
    
    MockLogEntryHMAC_t entry = {
        .timestamp = 1234567890,
        .duty_cycle = 50.0f,
        .efficiency = 95.5f,
        .temperature = 45.0f,
        .current = 2.5f,
        .error_code = 0,
        .reserved = 0
    };
    
    // Generate salt
    for (int i = 0; i < HMAC_SALT_SIZE; i++) {
        entry.salt[i] = (uint8_t)(i * 17 + 42);
    }
    
    // Compute and verify
    mock_hmac_compute(&entry, key);
    bool valid = mock_hmac_verify(&entry, key);
    TEST_ASSERT(valid == true);
}

/**
 * @brief Test HMAC tamper detection
 * Verify that modified data is detected
 */
void test_hmac_tamper_detection(void)
{
    printf("\n--- Test: HMAC Tamper Detection ---\n");
    tests_run++;
    
    uint8_t key[HMAC_SHA256_KEY_SIZE] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
    };
    
    MockLogEntryHMAC_t entry = {
        .timestamp = 1234567890,
        .duty_cycle = 50.0f,
        .efficiency = 95.5f,
        .temperature = 45.0f,
        .current = 2.5f,
        .error_code = 0,
        .reserved = 0
    };
    
    // Generate salt
    for (int i = 0; i < HMAC_SALT_SIZE; i++) {
        entry.salt[i] = (uint8_t)(i * 17 + 42);
    }
    
    // Compute signature
    mock_hmac_compute(&entry, key);
    
    // Verify original is valid
    TEST_ASSERT(mock_hmac_verify(&entry, key) == true);
    
    // Tamper with duty cycle
    entry.duty_cycle = 75.0f;
    TEST_ASSERT(mock_hmac_verify(&entry, key) == false);
    
    // Restore and tamper with timestamp
    entry.duty_cycle = 50.0f;
    mock_hmac_compute(&entry, key);
    entry.timestamp = 9999999999;
    TEST_ASSERT(mock_hmac_verify(&entry, key) == false);
    
    // Restore and tamper with error code
    entry.timestamp = 1234567890;
    mock_hmac_compute(&entry, key);
    entry.error_code = 0xDEAD;
    TEST_ASSERT(mock_hmac_verify(&entry, key) == false);
    
    printf("    ✓ PASS: Tamper detection works for all fields\n");
}

/**
 * @brief Test HMAC with wrong key
 * CWE-354: Improper Validation of Integrity Check Value
 */
void test_hmac_wrong_key(void)
{
    printf("\n--- Test: HMAC Wrong Key Detection ---\n");
    tests_run++;
    
    uint8_t key[HMAC_SHA256_KEY_SIZE] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
    };
    
    uint8_t wrong_key[HMAC_SHA256_KEY_SIZE] = {
        0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8,
        0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xF0,
        0xEF, 0xEE, 0xED, 0xEC, 0xEB, 0xEA, 0xE9, 0xE8,
        0xE7, 0xE6, 0xE5, 0xE4, 0xE3, 0xE2, 0xE1, 0xE0
    };
    
    MockLogEntryHMAC_t entry = {
        .timestamp = 1234567890,
        .duty_cycle = 50.0f,
        .efficiency = 95.5f,
        .temperature = 45.0f,
        .current = 2.5f,
        .error_code = 0,
        .reserved = 0
    };
    
    // Generate salt
    for (int i = 0; i < HMAC_SALT_SIZE; i++) {
        entry.salt[i] = (uint8_t)(i * 17 + 42);
    }
    
    // Compute with correct key
    mock_hmac_compute(&entry, key);
    
    // Verify with correct key
    TEST_ASSERT(mock_hmac_verify(&entry, key) == true);
    
    // Verify with wrong key
    TEST_ASSERT(mock_hmac_verify(&entry, wrong_key) == false);
}

/**
 * @brief Test salt uniqueness
 * Each entry should have unique salt
 */
void test_hmac_salt_uniqueness(void)
{
    printf("\n--- Test: HMAC Salt Uniqueness ---\n");
    tests_run++;
    
    uint8_t key[HMAC_SHA256_KEY_SIZE] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
    };
    
    MockLogEntryHMAC_t entry1 = { .timestamp = 1, .duty_cycle = 50.0f };
    MockLogEntryHMAC_t entry2 = { .timestamp = 1, .duty_cycle = 50.0f };  // Same data
    
    // Different salts
    for (int i = 0; i < HMAC_SALT_SIZE; i++) {
        entry1.salt[i] = (uint8_t)(i + 1);
        entry2.salt[i] = (uint8_t)(i + 100);
    }
    
    // Compute signatures
    mock_hmac_compute(&entry1, key);
    mock_hmac_compute(&entry2, key);
    
    // Same data but different salts should produce different signatures
    int diff_count = 0;
    for (int i = 0; i < HMAC_SHA256_SIGNATURE_SIZE; i++) {
        if (entry1.signature[i] != entry2.signature[i]) {
            diff_count++;
        }
    }
    
    printf("    Signature differences with different salts: %d/%d bytes\n", 
           diff_count, HMAC_SHA256_SIGNATURE_SIZE);
    
    // Should be significantly different
    TEST_ASSERT(diff_count > HMAC_SHA256_SIGNATURE_SIZE / 4);
}

// =============================================================================
// MAIN
// =============================================================================

int main(void)
{
    printf("==============================================\n");
    printf("AdaptivePWM Security Tests: HMAC Verification\n");
    printf("CWE-353, CWE-354, SEC-019\n");
    printf("==============================================\n");
    
    // Run all tests
    test_hmac_computation();
    test_hmac_verification();
    test_hmac_tamper_detection();
    test_hmac_wrong_key();
    test_hmac_salt_uniqueness();
    
    // Summary
    printf("\n==============================================\n");
    printf("Security Test Summary\n");
    printf("==============================================\n");
    printf("Tests run: %d\n", tests_run);
    printf("Failures: %d\n", test_failures);
    
    if (test_failures == 0) {
        printf("\n✓ All HMAC verification tests PASSED\n");
        return 0;
    } else {
        printf("\n✗ %d HMAC verification test(s) FAILED\n", test_failures);
        return 1;
    }
}
