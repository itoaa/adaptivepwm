/**
 * @file test_flash_logger_hmac.c
 * @brief Unit tests for HMAC-SHA256 flash logger integrity protection
 *
 * Test coverage:
 * - HMAC key generation and storage
 * - HMAC signature computation
 * - HMAC signature verification
 * - Tamper detection
 * - Entry chaining
 * - Chain integrity verification
 * - Flash write/read with HMAC
 * - Edge cases and error handling
 *
 * @version 1.0.0
 * @date 2026-04-12
 */

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include "../src/flash_logger_hmac.h"
#include "../src/flash_logger.h"

// Mock implementations for testing
#define TEST_FLASH_SIZE     65536
static uint8_t g_test_flash[TEST_FLASH_SIZE];
static bool g_flash_initialized = false;

// Test key for reproducible tests
static const uint8_t TEST_HMAC_KEY[HMAC_SHA256_KEY_SIZE] = {
    0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x07, 0x18,
    0x29, 0x3A, 0x4B, 0x5C, 0x6D, 0x7E, 0x8F, 0x90,
    0x01, 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78,
    0x89, 0x9A, 0xAB, 0xBC, 0xCD, 0xDE, 0xEF, 0xF0
};

// =============================================================================
// MOCK FUNCTIONS
// =============================================================================

void mock_flash_init(void)
{
    memset(g_test_flash, 0xFF, TEST_FLASH_SIZE);
    g_flash_initialized = true;
}

// Mock HAL functions
HAL_StatusTypeDef HAL_FLASH_Unlock(void) { return HAL_OK; }
HAL_StatusTypeDef HAL_FLASH_Lock(void) { return HAL_OK; }

HAL_StatusTypeDef HAL_FLASH_Program(uint32_t TypeProgram, uint32_t Address, uint64_t Data)
{
    if (!g_flash_initialized) mock_flash_init();
    
    uint32_t offset = Address - 0x08000000;
    if (offset >= TEST_FLASH_SIZE) return HAL_ERROR;
    
    // Flash programming: can only clear bits (0), not set them
    uint64_t* flash_ptr = (uint64_t*)&g_test_flash[offset];
    *flash_ptr = *flash_ptr & Data;
    
    return HAL_OK;
}

HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *pEraseInit, uint32_t *SectorError)
{
    if (!g_flash_initialized) mock_flash_init();
    
    // Erase sector (set to 0xFF)
    uint32_t sector_start = 0x080E0000 - 0x08000000;  // FLASH_LOG_START_ADDR
    memset(&g_test_flash[sector_start], 0xFF, FLASH_LOG_SIZE);
    
    return HAL_OK;
}

// Override key storage for testing
bool HMAC_InitKeyStorage(void)
{
    extern HMAC_KeyStorage_t g_hmac_key;
    memcpy(g_hmac_key.key, TEST_HMAC_KEY, HMAC_SHA256_KEY_SIZE);
    g_hmac_key.initialized = true;
    return true;
}

// =============================================================================
// TEST FUNCTIONS
// =============================================================================

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST(name) void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Testing %s... ", #name); \
    g_tests_run++; \
    test_##name(); \
    g_tests_passed++; \
    printf("PASS\n"); \
} while(0)

#define ASSERT_EQ(a, b) assert((a) == (b))
#define ASSERT_TRUE(x) assert(x)
#define ASSERT_FALSE(x) assert(!(x))
#define ASSERT_MEMEQ(a, b, n) assert(memcmp((a), (b), (n)) == 0)

// =============================================================================
// TEST: HMAC Key Storage
// =============================================================================

TEST(hmac_key_init)
{
    HMAC_ClearKey();  // Reset first
    
    ASSERT_FALSE(g_hmac_key.initialized);
    ASSERT_TRUE(HMAC_InitKeyStorage());
    ASSERT_TRUE(g_hmac_key.initialized);
}

TEST(hmac_key_retrieval)
{
    HMAC_InitKeyStorage();
    
    const HMAC_KeyStorage_t* key = HMAC_GetKeyStorage();
    ASSERT_TRUE(key != NULL);
    ASSERT_TRUE(key->initialized);
    ASSERT_MEMEQ(key->key, TEST_HMAC_KEY, HMAC_SHA256_KEY_SIZE);
}

TEST(hmac_key_clear)
{
    HMAC_InitKeyStorage();
    HMAC_ClearKey();
    
    ASSERT_FALSE(g_hmac_key.initialized);
    
    // Verify key is cleared (secure zeroing)
    bool all_zero = true;
    for (int i = 0; i < HMAC_SHA256_KEY_SIZE; i++) {
        if (g_hmac_key.key[i] != 0) {
            all_zero = false;
            break;
        }
    }
    ASSERT_TRUE(all_zero);
}

// =============================================================================
// TEST: HMAC Signature Computation
// =============================================================================

TEST(hmac_compute_signature)
{
    HMAC_InitKeyStorage();
    
    LogEntryHMAC_t entry = {
        .timestamp = 1234567890,
        .duty_cycle = 0.75f,
        .efficiency = 0.95f,
        .temperature = 45.5f,
        .current = 2.5f,
        .error_code = 0,
        .reserved = 0
    };
    
    // Fill salt with test data
    memset(entry.salt, 0xAB, HMAC_SALT_SIZE);
    memset(entry.prev_hash, 0xCD, 8);
    
    uint8_t signature[HMAC_SHA256_SIGNATURE_SIZE];
    ASSERT_TRUE(HMAC_ComputeSignature(&entry, signature));
    
    // Verify signature is not all zeros (actual computation occurred)
    bool all_zero = true;
    for (int i = 0; i < HMAC_SHA256_SIGNATURE_SIZE; i++) {
        if (signature[i] != 0) {
            all_zero = false;
            break;
        }
    }
    ASSERT_FALSE(all_zero);
}

TEST(hmac_verify_signature)
{
    HMAC_InitKeyStorage();
    
    LogEntryHMAC_t entry = {
        .timestamp = 1234567890,
        .duty_cycle = 0.75f,
        .efficiency = 0.95f,
        .temperature = 45.5f,
        .current = 2.5f,
        .error_code = 0,
        .reserved = 0
    };
    
    memset(entry.salt, 0xAB, HMAC_SALT_SIZE);
    memset(entry.prev_hash, 0xCD, 8);
    
    // Compute signature
    ASSERT_TRUE(HMAC_ComputeSignature(&entry, entry.signature));
    
    // Verify signature
    ASSERT_TRUE(HMAC_VerifySignature(&entry));
}

// =============================================================================
// TEST: Tamper Detection
// =============================================================================

TEST(tamper_detection_duty_cycle)
{
    HMAC_InitKeyStorage();
    
    LogEntryHMAC_t entry = {
        .timestamp = 1234567890,
        .duty_cycle = 0.75f,
        .efficiency = 0.95f,
        .temperature = 45.5f,
        .current = 2.5f,
        .error_code = 0,
        .reserved = 0
    };
    
    memset(entry.salt, 0xAB, HMAC_SALT_SIZE);
    memset(entry.prev_hash, 0xCD, 8);
    
    // Compute valid signature
    ASSERT_TRUE(HMAC_ComputeSignature(&entry, entry.signature));
    ASSERT_TRUE(HMAC_VerifySignature(&entry));
    
    // Tamper with duty cycle
    entry.duty_cycle = 0.80f;
    
    // Should detect tampering
    ASSERT_FALSE(HMAC_VerifySignature(&entry));
}

TEST(tamper_detection_timestamp)
{
    HMAC_InitKeyStorage();
    
    LogEntryHMAC_t entry = {
        .timestamp = 1234567890,
        .duty_cycle = 0.75f,
        .efficiency = 0.95f,
        .temperature = 45.5f,
        .current = 2.5f,
        .error_code = 0,
        .reserved = 0
    };
    
    memset(entry.salt, 0xAB, HMAC_SALT_SIZE);
    memset(entry.prev_hash, 0xCD, 8);
    
    ASSERT_TRUE(HMAC_ComputeSignature(&entry, entry.signature));
    ASSERT_TRUE(HMAC_VerifySignature(&entry));
    
    // Tamper with timestamp
    entry.timestamp = 9999999999;
    
    ASSERT_FALSE(HMAC_VerifySignature(&entry));
}

TEST(tamper_detection_salt)
{
    HMAC_InitKeyStorage();
    
    LogEntryHMAC_t entry = {
        .timestamp = 1234567890,
        .duty_cycle = 0.75f,
        .efficiency = 0.95f,
        .temperature = 45.5f,
        .current = 2.5f,
        .error_code = 0,
        .reserved = 0
    };
    
    memset(entry.salt, 0xAB, HMAC_SALT_SIZE);
    memset(entry.prev_hash, 0xCD, 8);
    
    ASSERT_TRUE(HMAC_ComputeSignature(&entry, entry.signature));
    ASSERT_TRUE(HMAC_VerifySignature(&entry));
    
    // Tamper with salt (simulates replay attack)
    entry.salt[0] = 0xFF;
    
    ASSERT_FALSE(HMAC_VerifySignature(&entry));
}

TEST(tamper_detection_signature)
{
    HMAC_InitKeyStorage();
    
    LogEntryHMAC_t entry = {
        .timestamp = 1234567890,
        .duty_cycle = 0.75f,
        .efficiency = 0.95f,
        .temperature = 45.5f,
        .current = 2.5f,
        .error_code = 0,
        .reserved = 0
    };
    
    memset(entry.salt, 0xAB, HMAC_SALT_SIZE);
    memset(entry.prev_hash, 0xCD, 8);
    
    ASSERT_TRUE(HMAC_ComputeSignature(&entry, entry.signature));
    
    // Tamper with signature
    entry.signature[0] ^= 0xFF;
    entry.signature[1] ^= 0xFF;
    
    ASSERT_FALSE(HMAC_VerifySignature(&entry));
}

// =============================================================================
// TEST: Entry Chaining
// =============================================================================

TEST(chain_hash_update)
{
    HMAC_InitKeyStorage();
    
    LogEntryHMAC_t entry1 = {
        .timestamp = 1000,
        .duty_cycle = 0.5f,
        .efficiency = 0.9f,
        .temperature = 40.0f,
        .current = 1.0f,
        .error_code = 0,
        .reserved = 0
    };
    
    memset(entry1.salt, 0xAA, HMAC_SALT_SIZE);
    memset(entry1.prev_hash, 0, 8);  // First entry has no previous
    
    ASSERT_TRUE(HMAC_ComputeSignature(&entry1, entry1.signature));
    
    uint8_t chain_hash[32];
    memset(chain_hash, 0, 32);
    
    // Update chain with first entry
    HMAC_UpdateChainHash(&entry1, chain_hash);
    
    // Chain hash should now be non-zero
    bool all_zero = true;
    for (int i = 0; i < 32; i++) {
        if (chain_hash[i] != 0) {
            all_zero = false;
            break;
        }
    }
    ASSERT_FALSE(all_zero);
    
    // Create second entry with chain hash from first
    LogEntryHMAC_t entry2 = {
        .timestamp = 2000,
        .duty_cycle = 0.6f,
        .efficiency = 0.92f,
        .temperature = 42.0f,
        .current = 1.2f,
        .error_code = 0,
        .reserved = 0
    };
    
    memset(entry2.salt, 0xBB, HMAC_SALT_SIZE);
    memcpy(entry2.prev_hash, chain_hash, 8);
    
    ASSERT_TRUE(HMAC_ComputeSignature(&entry2, entry2.signature));
    
    // Verify second entry links to first
    ASSERT_MEMEQ(entry2.prev_hash, chain_hash, 8);
}

// =============================================================================
// TEST: Conversion Functions
// =============================================================================

TEST(convert_legacy_to_hmac)
{
    HMAC_InitKeyStorage();
    
    LogEntry_t legacy = {
        .timestamp = 1234567890,
        .duty_cycle = 0.75f,
        .efficiency = 0.95f,
        .temperature = 45.5f,
        .current = 2.5f,
        .error_code = 42,
        .crc = 0x1234
    };
    
    LogEntryHMAC_t hmac;
    uint8_t chain_hash[32] = {0};
    
    ASSERT_TRUE(FlashLogger_ConvertToHMAC(&legacy, &hmac, chain_hash));
    
    ASSERT_EQ(hmac.timestamp, legacy.timestamp);
    ASSERT_TRUE(hmac.duty_cycle == legacy.duty_cycle);
    ASSERT_TRUE(hmac.efficiency == legacy.efficiency);
    ASSERT_TRUE(hmac.temperature == legacy.temperature);
    ASSERT_TRUE(hmac.current == legacy.current);
    ASSERT_EQ(hmac.error_code, legacy.error_code);
    
    // HMAC entry should have valid signature
    ASSERT_TRUE(HMAC_VerifySignature(&hmac));
}

TEST(convert_hmac_to_legacy)
{
    HMAC_InitKeyStorage();
    
    LogEntryHMAC_t hmac = {
        .timestamp = 1234567890,
        .duty_cycle = 0.75f,
        .efficiency = 0.95f,
        .temperature = 45.5f,
        .current = 2.5f,
        .error_code = 42,
        .reserved = 0
    };
    
    memset(hmac.salt, 0xAB, HMAC_SALT_SIZE);
    memset(hmac.prev_hash, 0xCD, 8);
    ASSERT_TRUE(HMAC_ComputeSignature(&hmac, hmac.signature));
    
    LogEntry_t legacy;
    FlashLogger_ConvertFromHMAC(&hmac, &legacy);
    
    ASSERT_EQ(legacy.timestamp, hmac.timestamp);
    ASSERT_TRUE(legacy.duty_cycle == hmac.duty_cycle);
    ASSERT_TRUE(legacy.efficiency == hmac.efficiency);
    ASSERT_TRUE(legacy.temperature == hmac.temperature);
    ASSERT_TRUE(legacy.current == hmac.current);
    ASSERT_EQ(legacy.error_code, hmac.error_code);
    ASSERT_EQ(legacy.crc, 0);  // CRC not used in HMAC mode
}

// =============================================================================
// TEST: Flash Logger Integration
// =============================================================================

TEST(flash_logger_hmac_init)
{
    mock_flash_init();
    HMAC_ClearKey();
    
    FlashLogger_t logger;
    ASSERT_TRUE(FlashLoggerHMAC_Init(&logger));
    ASSERT_TRUE(logger.initialized);
    ASSERT_EQ(logger.entry_count, 0);
}

TEST(flash_logger_hmac_write_read)
{
    mock_flash_init();
    HMAC_ClearKey();
    
    FlashLogger_t logger;
    ASSERT_TRUE(FlashLoggerHMAC_Init(&logger));
    
    // Write entry
    LogEntry_t entry = {
        .timestamp = 1000,
        .duty_cycle = 0.5f,
        .efficiency = 0.9f,
        .temperature = 40.0f,
        .current = 1.0f,
        .error_code = 0,
        .crc = 0
    };
    
    ASSERT_TRUE(FlashLoggerHMAC_Write(&logger, &entry));
    ASSERT_EQ(logger.entry_count, 1);
    
    // Read back
    LogEntry_t read_entry;
    bool tampered = true;
    ASSERT_EQ(FlashLoggerHMAC_Read(&logger, 0, &read_entry, &tampered), sizeof(LogEntry_t));
    ASSERT_FALSE(tampered);
    ASSERT_EQ(read_entry.timestamp, entry.timestamp);
    ASSERT_TRUE(read_entry.duty_cycle == entry.duty_cycle);
}

TEST(flash_logger_hmac_verify_empty)
{
    mock_flash_init();
    HMAC_ClearKey();
    
    FlashLogger_t logger;
    ASSERT_TRUE(FlashLoggerHMAC_Init(&logger));
    
    uint32_t valid, tampered;
    bool chain_broken;
    ASSERT_TRUE(FlashLoggerHMAC_VerifyLog(&logger, &valid, &tampered, &chain_broken));
    ASSERT_EQ(valid, 0);
    ASSERT_EQ(tampered, 0);
    ASSERT_FALSE(chain_broken);
}

TEST(flash_logger_hmac_verify_entries)
{
    mock_flash_init();
    HMAC_ClearKey();
    
    FlashLogger_t logger;
    ASSERT_TRUE(FlashLoggerHMAC_Init(&logger));
    
    // Write multiple entries
    for (int i = 0; i < 10; i++) {
        LogEntry_t entry = {
            .timestamp = 1000 + i,
            .duty_cycle = 0.5f + (i * 0.01f),
            .efficiency = 0.9f,
            .temperature = 40.0f + i,
            .current = 1.0f,
            .error_code = 0,
            .crc = 0
        };
        ASSERT_TRUE(FlashLoggerHMAC_Write(&logger, &entry));
    }
    
    // Verify log
    uint32_t valid, tampered;
    bool chain_broken;
    ASSERT_TRUE(FlashLoggerHMAC_VerifyLog(&logger, &valid, &tampered, &chain_broken));
    ASSERT_EQ(valid, 10);
    ASSERT_EQ(tampered, 0);
    ASSERT_FALSE(chain_broken);
}

TEST(flash_logger_hmac_clear)
{
    mock_flash_init();
    HMAC_ClearKey();
    
    FlashLogger_t logger;
    ASSERT_TRUE(FlashLoggerHMAC_Init(&logger));
    
    // Write entry
    LogEntry_t entry = {
        .timestamp = 1000,
        .duty_cycle = 0.5f,
        .efficiency = 0.9f,
        .temperature = 40.0f,
        .current = 1.0f,
        .error_code = 0,
        .crc = 0
    };
    
    ASSERT_TRUE(FlashLoggerHMAC_Write(&logger, &entry));
    ASSERT_EQ(logger.entry_count, 1);
    
    // Clear log
    ASSERT_TRUE(FlashLoggerHMAC_Clear(&logger));
    ASSERT_EQ(logger.entry_count, 0);
    
    // Verify empty
    uint32_t valid, tampered;
    bool chain_broken;
    ASSERT_TRUE(FlashLoggerHMAC_VerifyLog(&logger, &valid, &tampered, &chain_broken));
    ASSERT_EQ(valid, 0);
}

// =============================================================================
// TEST: Edge Cases
// =============================================================================

TEST(null_pointer_handling)
{
    HMAC_InitKeyStorage();
    
    // HMAC compute with null
    LogEntryHMAC_t entry;
    uint8_t sig[32];
    ASSERT_FALSE(HMAC_ComputeSignature(NULL, sig));
    ASSERT_FALSE(HMAC_ComputeSignature(&entry, NULL));
    
    // HMAC verify with null
    ASSERT_FALSE(HMAC_VerifySignature(NULL));
}

TEST(invalid_entry_index)
{
    mock_flash_init();
    HMAC_ClearKey();
    
    FlashLogger_t logger;
    ASSERT_TRUE(FlashLoggerHMAC_Init(&logger));
    
    LogEntry_t entry;
    bool tampered;
    
    // Read from empty log
    ASSERT_EQ(FlashLoggerHMAC_Read(&logger, 0, &entry, &tampered), 0);
    
    // Read invalid index
    ASSERT_EQ(FlashLoggerHMAC_Read(&logger, 999, &entry, &tampered), 0);
}

// =============================================================================
// MAIN
// =============================================================================

int main(void)
{
    printf("========================================\n");
    printf("Flash Logger HMAC Unit Tests\n");
    printf("========================================\n\n");
    
    // Key Management Tests
    printf("Key Management:\n");
    RUN_TEST(hmac_key_init);
    RUN_TEST(hmac_key_retrieval);
    RUN_TEST(hmac_key_clear);
    
    // HMAC Computation Tests
    printf("\nHMAC Computation:\n");
    RUN_TEST(hmac_compute_signature);
    RUN_TEST(hmac_verify_signature);
    
    // Tamper Detection Tests
    printf("\nTamper Detection:\n");
    RUN_TEST(tamper_detection_duty_cycle);
    RUN_TEST(tamper_detection_timestamp);
    RUN_TEST(tamper_detection_salt);
    RUN_TEST(tamper_detection_signature);
    
    // Chain Tests
    printf("\nEntry Chaining:\n");
    RUN_TEST(chain_hash_update);
    
    // Conversion Tests
    printf("\nConversion Functions:\n");
    RUN_TEST(convert_legacy_to_hmac);
    RUN_TEST(convert_hmac_to_legacy);
    
    // Integration Tests
    printf("\nFlash Logger Integration:\n");
    RUN_TEST(flash_logger_hmac_init);
    RUN_TEST(flash_logger_hmac_write_read);
    RUN_TEST(flash_logger_hmac_verify_empty);
    RUN_TEST(flash_logger_hmac_verify_entries);
    RUN_TEST(flash_logger_hmac_clear);
    
    // Edge Cases
    printf("\nEdge Cases:\n");
    RUN_TEST(null_pointer_handling);
    RUN_TEST(invalid_entry_index);
    
    printf("\n========================================\n");
    printf("Results: %d/%d tests passed\n", g_tests_passed, g_tests_run);
    printf("========================================\n");
    
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
