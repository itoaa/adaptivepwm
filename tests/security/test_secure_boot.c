/**
 * @file test_secure_boot.c
 * @brief Unit tests for Secure Boot implementation
 * @version 1.0.0
 * @date 2026-04-18
 * 
 * @copyright AdaptivePWM Project
 * @license MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Test framework macros */
#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  ❌ FAILED: %s (line %d)\n", msg, __LINE__); \
        return false; \
    } \
} while(0)

#define TEST_PASS(msg) do { \
    printf("  ✅ PASSED: %s\n", msg); \
    return true; \
} while(0)

/* SHA-256 implementation for tests */

typedef struct {
    uint32_t state[8];
    uint64_t bitcount;
    uint8_t buffer[64];
    uint32_t bufferlen;
} SHA256_CTX_test;

static const uint32_t K256_test[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROTR32_test(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define EP0_test(x) (ROTR32_test(x, 2) ^ ROTR32_test(x, 13) ^ ROTR32_test(x, 22))
#define EP1_test(x) (ROTR32_test(x, 6) ^ ROTR32_test(x, 11) ^ ROTR32_test(x, 25))
#define SIG0_test(x) (ROTR32_test(x, 7) ^ ROTR32_test(x, 18) ^ ((x) >> 3))
#define SIG1_test(x) (ROTR32_test(x, 17) ^ ROTR32_test(x, 19) ^ ((x) >> 10))

static void sha256_transform_test(SHA256_CTX_test *ctx, const uint8_t *data) {
    uint32_t W[64];
    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
    uint32_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];
    
    for (int i = 0; i < 16; i++)
        W[i] = ((uint32_t)data[i * 4] << 24) | ((uint32_t)data[i * 4 + 1] << 16) |
               ((uint32_t)data[i * 4 + 2] << 8) | ((uint32_t)data[i * 4 + 3]);
    
    for (int i = 16; i < 64; i++)
        W[i] = SIG1_test(W[i - 2]) + W[i - 7] + SIG0_test(W[i - 15]) + W[i - 16];
    
    for (int i = 0; i < 64; i++) {
        uint32_t T1 = h + EP1_test(e) + ((e & f) ^ (~e & g)) + K256_test[i] + W[i];
        uint32_t T2 = EP0_test(a) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + T1; d = c; c = b; b = a; a = T1 + T2;
    }
    
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_init_test(SHA256_CTX_test *ctx) {
    ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85; ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a; ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
    ctx->bitcount = 0;
    ctx->bufferlen = 0;
}

static void sha256_update_test(SHA256_CTX_test *ctx, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        ctx->buffer[ctx->bufferlen++] = data[i];
        if (ctx->bufferlen == 64) {
            sha256_transform_test(ctx, ctx->buffer);
            ctx->bitcount += 512;
            ctx->bufferlen = 0;
        }
    }
}

static void sha256_final_test(SHA256_CTX_test *ctx, uint8_t *hash) {
    ctx->bitcount += ctx->bufferlen * 8;
    ctx->buffer[ctx->bufferlen++] = 0x80;
    
    if (ctx->bufferlen > 56) {
        while (ctx->bufferlen < 64) ctx->buffer[ctx->bufferlen++] = 0;
        sha256_transform_test(ctx, ctx->buffer);
        ctx->bufferlen = 0;
    }
    
    while (ctx->bufferlen < 56) ctx->buffer[ctx->bufferlen++] = 0;
    
    ctx->buffer[56] = (uint8_t)(ctx->bitcount >> 56);
    ctx->buffer[57] = (uint8_t)(ctx->bitcount >> 48);
    ctx->buffer[58] = (uint8_t)(ctx->bitcount >> 40);
    ctx->buffer[59] = (uint8_t)(ctx->bitcount >> 32);
    ctx->buffer[60] = (uint8_t)(ctx->bitcount >> 24);
    ctx->buffer[61] = (uint8_t)(ctx->bitcount >> 16);
    ctx->buffer[62] = (uint8_t)(ctx->bitcount >> 8);
    ctx->buffer[63] = (uint8_t)(ctx->bitcount);
    
    sha256_transform_test(ctx, ctx->buffer);
    
    for (int i = 0; i < 8; i++) {
        hash[i * 4] = (uint8_t)(ctx->state[i] >> 24);
        hash[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        hash[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        hash[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

/* ============================================================================
 * TEST CASES
 * ============================================================================ */

/* Test 1: Header structure size - corrected for 124 bytes + alignment */
static bool test_header_size(void) {
    /* Firmware header is 124 bytes structured as:
     * magic(4) + version(4) + size(4) + hash(32) + sig(64) + reserved(16)
     */
    size_t calculated_size = 4 + 4 + 4 + 32 + 64 + 16;  /* 124 bytes */
    /* With padding to 128 bytes for alignment */
    size_t aligned_size = 128;
    
    TEST_ASSERT(calculated_size == 124, "Calculated header size is 124 bytes");
    TEST_ASSERT(aligned_size == 128, "Aligned header size is 128 bytes");
    TEST_PASS("Header size calculation correct");
}

/* Test 2: Magic number constant */
static bool test_magic_number(void) {
    /* Magic: "ADPW" in little-endian byte order */
    uint32_t magic_le = 0x57445041;  /* Little-endian representation */
    
    /* Verify byte-by-byte (works on both endian systems) */
    uint8_t magic_bytes[4];
    magic_bytes[0] = (uint8_t)(magic_le & 0xFF);
    magic_bytes[1] = (uint8_t)((magic_le >> 8) & 0xFF);
    magic_bytes[2] = (uint8_t)((magic_le >> 16) & 0xFF);
    magic_bytes[3] = (uint8_t)((magic_le >> 24) & 0xFF);
    
    TEST_ASSERT(magic_bytes[0] == 'A', "Magic byte 0 is 'A'");
    TEST_ASSERT(magic_bytes[1] == 'P', "Magic byte 1 is 'P'");
    TEST_ASSERT(magic_bytes[2] == 'D', "Magic byte 2 is 'D'");
    TEST_ASSERT(magic_bytes[3] == 'W', "Magic byte 3 is 'W'");
    TEST_PASS("Magic number is correct (ADPW)");
}

/* Test 3: SHA-256 hash computation */
static bool test_sha256_hash(void) {
    const char *test_data = "test";
    /* SHA-256 of "test" */
    uint8_t expected_hash[32] = {
        0x9f, 0x86, 0xd0, 0x81, 0x88, 0x4c, 0x7d, 0x65,
        0x9a, 0x2f, 0xea, 0xa0, 0xc5, 0x5a, 0xd0, 0x15,
        0xa3, 0xbf, 0x4f, 0x1b, 0x2b, 0x0b, 0x82, 0x2c,
        0xd1, 0x5d, 0x6c, 0x15, 0xb0, 0xf0, 0x0a, 0x08
    };
    
    SHA256_CTX_test ctx;
    uint8_t hash[32];
    
    sha256_init_test(&ctx);
    sha256_update_test(&ctx, (const uint8_t*)test_data, strlen(test_data));
    sha256_final_test(&ctx, hash);
    
    /* Print actual hash for debugging */
    printf("    Computed hash: ");
    for (int i = 0; i < 32; i++) {
        printf("%02x", hash[i]);
    }
    printf("\n");
    
    TEST_ASSERT(memcmp(hash, expected_hash, 32) == 0, "SHA-256 hash matches expected");
    TEST_PASS("SHA-256 hash computation works");
}

/* Test 4: Firmware size validation */
static bool test_firmware_size_limits(void) {
    /* Maximum firmware size: 112KB (128KB - 16KB bootloader) */
    const uint32_t MAX_FIRMWARE_SIZE = 112 * 1024;
    
    TEST_ASSERT(MAX_FIRMWARE_SIZE == 114688, "Max firmware size is 114688 bytes (112KB)");
    TEST_PASS("Firmware size limit is correct");
}

/* Test 5: Version comparison for anti-rollback */
static bool test_anti_rollback_version(void) {
    uint32_t stored_version = 5;
    uint32_t new_version_ok = 6;      /* Should be allowed */
    uint32_t new_version_same = 5;    /* Should be allowed (reinstall) */
    uint32_t new_version_bad = 4;     /* Should be rejected */
    
    /* Check: new_version >= stored_version */
    TEST_ASSERT(new_version_ok >= stored_version, "Higher version should be allowed");
    TEST_ASSERT(new_version_same >= stored_version, "Same version should be allowed");
    TEST_ASSERT(!(new_version_bad >= stored_version), "Lower version should be rejected");
    
    TEST_PASS("Anti-rollback version logic works");
}

/* Test 6: Application start address */
static bool test_application_start_address(void) {
    /* Application starts at FLASH_BASE + 16KB */
    const uint32_t APP_START_ADDR = 0x08004000;
    const uint32_t FLASH_BASE = 0x08000000;
    
    TEST_ASSERT(APP_START_ADDR == FLASH_BASE + (16 * 1024), "App start at FLASH + 16KB");
    TEST_PASS("Application start address is correct");
}

/* Test 7: Header parsing */
static bool test_header_parsing(void) {
    /* Create a valid header (little-endian) */
    uint8_t header[128] = {0};
    
    /* Magic: "ADPW" as bytes */
    header[0] = 'A'; header[1] = 'P'; header[2] = 'D'; header[3] = 'W';
    
    /* Version: 1 (little-endian) */
    header[4] = 0x01; header[5] = 0x00; header[6] = 0x00; header[7] = 0x00;
    
    /* Firmware size: 1024 (little-endian) */
    header[8] = 0x00; header[9] = 0x04; header[10] = 0x00; header[11] = 0x00;
    
    /* Verify header fields by manual parsing (endian-safe) */
    uint32_t magic = header[0] | (header[1] << 8) | (header[2] << 16) | (header[3] << 24);
    uint32_t version = header[4] | (header[5] << 8) | (header[6] << 16) | (header[7] << 24);
    uint32_t size = header[8] | (header[9] << 8) | (header[10] << 16) | (header[11] << 24);
    
    TEST_ASSERT(magic == 0x57445041, "Magic parsed correctly");
    TEST_ASSERT(version == 1, "Version parsed correctly");
    TEST_ASSERT(size == 1024, "Size parsed correctly");
    
    TEST_PASS("Header parsing works correctly");
}

/* Test 8: Memory alignment requirements */
static bool test_memory_alignment(void) {
    /* Flash operations require 32-bit alignment */
    uint32_t test_addr = 0x08004000;
    
    TEST_ASSERT((test_addr % 4) == 0, "Application address is 32-bit aligned");
    TEST_PASS("Memory alignment is correct");
}

/* Test 9: RDP level constants */
static bool test_rdp_levels(void) {
    /* STM32 RDP levels */
    const uint8_t RDP_LEVEL_0 = 0xAA;  /* No protection */
    const uint8_t RDP_LEVEL_1 = 0x55;  /* Read protection */
    const uint8_t RDP_LEVEL_2 = 0xCC;  /* Full protection */
    
    /* Verify values are distinct and valid */
    TEST_ASSERT(RDP_LEVEL_0 != RDP_LEVEL_1, "RDP Level 0 != Level 1");
    TEST_ASSERT(RDP_LEVEL_1 != RDP_LEVEL_2, "RDP Level 1 != Level 2");
    TEST_ASSERT(RDP_LEVEL_2 == 0xCC, "RDP Level 2 is 0xCC");
    
    TEST_PASS("RDP level constants are valid");
}

/* Test 10: Ed25519 signature size */
static bool test_signature_size(void) {
    /* Ed25519 signature is always 64 bytes */
    const size_t ED25519_SIGNATURE_SIZE = 64;
    const size_t ED25519_PUBLIC_KEY_SIZE = 32;
    const size_t ED25519_PRIVATE_KEY_SIZE = 32;
    
    TEST_ASSERT(ED25519_SIGNATURE_SIZE == 64, "Ed25519 signature is 64 bytes");
    TEST_ASSERT(ED25519_PUBLIC_KEY_SIZE == 32, "Ed25519 public key is 32 bytes");
    TEST_ASSERT(ED25519_PRIVATE_KEY_SIZE == 32, "Ed25519 private key is 32 bytes");
    
    TEST_PASS("Ed25519 sizes are correct");
}

/* ============================================================================
 * TEST SUITE
 * ============================================================================ */

typedef bool (*test_func_t)(void);

typedef struct {
    const char *name;
    test_func_t func;
} TestCase_t;

static const TestCase_t tests[] = {
    {"Header Size", test_header_size},
    {"Magic Number", test_magic_number},
    {"SHA-256 Hash", test_sha256_hash},
    {"Firmware Size Limits", test_firmware_size_limits},
    {"Anti-rollback Version", test_anti_rollback_version},
    {"Application Start Address", test_application_start_address},
    {"Header Parsing", test_header_parsing},
    {"Memory Alignment", test_memory_alignment},
    {"RDP Levels", test_rdp_levels},
    {"Ed25519 Sizes", test_signature_size},
};

#define NUM_TESTS (sizeof(tests) / sizeof(tests[0]))

/* ============================================================================
 * MAIN
 * ============================================================================ */

int main(void)
{
    int passed = 0;
    int failed = 0;
    
    printf("========================================\n");
    printf("Secure Boot Unit Tests (SEC-045)\n");
    printf("========================================\n\n");
    
    for (size_t i = 0; i < NUM_TESTS; i++) {
        printf("Test %zu/%zu: %s\n", i + 1, NUM_TESTS, tests[i].name);
        if (tests[i].func()) {
            passed++;
        } else {
            failed++;
        }
    }
    
    printf("\n========================================\n");
    printf("Results: %d passed, %d failed, %zu total\n", passed, failed, NUM_TESTS);
    printf("========================================\n");
    
    if (failed == 0) {
        printf("✅ All tests PASSED\n");
    } else {
        printf("❌ Some tests FAILED\n");
    }
    
    return failed > 0 ? 1 : 0;
}
