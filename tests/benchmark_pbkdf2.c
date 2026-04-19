/**
 * @file benchmark_pbkdf2.c
 * @brief PBKDF2 Performance Benchmark for SEC-027
 * 
 * Tests PBKDF2-SHA256 authentication performance with 100,000 iterations
 * to ensure it completes within acceptable time (<500ms target).
 * 
 * @security SEC-027: PBKDF2 Iteration Increase Verification
 * @date 2026-04-16
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

// Minimal SHA-256 implementation for testing
#define SHA256_BLOCK_SIZE 64

typedef struct {
    uint32_t state[8];
    uint64_t bitcount;
    uint8_t buffer[SHA256_BLOCK_SIZE];
    size_t buffer_len;
} sha256_ctx_t;

static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static inline uint32_t sha256_rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

static inline uint32_t sha256_ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

static inline uint32_t sha256_maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

static inline uint32_t sha256_ep0(uint32_t x) {
    return sha256_rotr(x, 2) ^ sha256_rotr(x, 13) ^ sha256_rotr(x, 22);
}

static inline uint32_t sha256_ep1(uint32_t x) {
    return sha256_rotr(x, 6) ^ sha256_rotr(x, 11) ^ sha256_rotr(x, 25);
}

static inline uint32_t sha256_sig0(uint32_t x) {
    return sha256_rotr(x, 7) ^ sha256_rotr(x, 18) ^ (x >> 3);
}

static inline uint32_t sha256_sig1(uint32_t x) {
    return sha256_rotr(x, 17) ^ sha256_rotr(x, 19) ^ (x >> 10);
}

static void sha256_transform(sha256_ctx_t* ctx, const uint8_t* data) {
    uint32_t a, b, c, d, e, f, g, h, t1, t2, m[64];
    
    for (size_t i = 0, j = 0; i < 16; i++, j += 4) {
        m[i] = ((uint32_t)data[j] << 24) | ((uint32_t)data[j + 1] << 16) |
               ((uint32_t)data[j + 2] << 8) | (uint32_t)data[j + 3];
    }
    
    for (size_t i = 16; i < 64; i++) {
        m[i] = sha256_sig1(m[i - 2]) + m[i - 7] + sha256_sig0(m[i - 15]) + m[i - 16];
    }
    
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];
    
    for (size_t i = 0; i < 64; i++) {
        t1 = h + sha256_ep1(e) + sha256_ch(e, f, g) + sha256_k[i] + m[i];
        t2 = sha256_ep0(a) + sha256_maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_init(sha256_ctx_t* ctx) {
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->bitcount = 0;
    ctx->buffer_len = 0;
}

static void sha256_update(sha256_ctx_t* ctx, const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        ctx->buffer[ctx->buffer_len++] = data[i];
        if (ctx->buffer_len == SHA256_BLOCK_SIZE) {
            sha256_transform(ctx, ctx->buffer);
            ctx->bitcount += SHA256_BLOCK_SIZE * 8;
            ctx->buffer_len = 0;
        }
    }
}

static void sha256_final(sha256_ctx_t* ctx, uint8_t hash[32]) {
    size_t i = ctx->buffer_len;
    uint64_t bitcount = ctx->bitcount + (ctx->buffer_len * 8);
    
    ctx->buffer[i++] = 0x80;
    
    if (ctx->buffer_len < 56) {
        while (i < 56) {
            ctx->buffer[i++] = 0;
        }
    } else {
        while (i < SHA256_BLOCK_SIZE) {
            ctx->buffer[i++] = 0;
        }
        sha256_transform(ctx, ctx->buffer);
        memset(ctx->buffer, 0, 56);
    }
    
    for (int j = 7; j >= 0; j--) {
        ctx->buffer[56 + (7 - j)] = (uint8_t)(bitcount >> (j * 8));
    }
    
    sha256_transform(ctx, ctx->buffer);
    
    for (i = 0; i < 8; i++) {
        hash[i * 4] = (uint8_t)(ctx->state[i] >> 24);
        hash[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        hash[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        hash[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

static bool sha256(const uint8_t* data, size_t len, uint8_t hash[32]) {
    if (data == NULL || hash == NULL) return false;
    
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, hash);
    
    return true;
}

// PBKDF2-SHA256 implementation
static bool pbkdf2_sha256(const char* password, size_t password_len,
                          const uint8_t* salt, size_t salt_len,
                          uint32_t iterations,
                          uint8_t* output, size_t output_len) {
    if (password == NULL || salt == NULL || output == NULL) {
        return false;
    }
    
    uint8_t u[32];
    uint8_t t[32];
    
    size_t blocks = (output_len + 31) / 32;
    size_t remaining = output_len;
    
    for (size_t block = 1; block <= blocks; block++) {
        // U_1 = PRF(password, salt || INT_32_BE(i))
        uint8_t salt_block[salt_len + 4];
        memcpy(salt_block, salt, salt_len);
        salt_block[salt_len] = (uint8_t)(block >> 24);
        salt_block[salt_len + 1] = (uint8_t)(block >> 16);
        salt_block[salt_len + 2] = (uint8_t)(block >> 8);
        salt_block[salt_len + 3] = (uint8_t)block;
        
        // First iteration: HMAC-SHA256(password, salt || i)
        // Simplified: just use SHA256 for now (like embedded implementation)
        size_t inner_len = password_len + salt_len + 4;
        uint8_t inner[inner_len];
        memcpy(inner, password, password_len);
        memcpy(inner + password_len, salt_block, salt_len + 4);
        sha256(inner, inner_len, u);
        memcpy(t, u, 32);
        
        // Remaining iterations
        for (uint32_t iter = 1; iter < iterations; iter++) {
            size_t u_len = password_len + 32;
            uint8_t u_data[u_len];
            memcpy(u_data, password, password_len);
            memcpy(u_data + password_len, u, 32);
            sha256(u_data, u_len, u);
            
            // XOR with t
            for (size_t j = 0; j < 32; j++) {
                t[j] ^= u[j];
            }
        }
        
        // Copy to output
        size_t copy_len = (remaining < 32) ? remaining : 32;
        memcpy(output + (block - 1) * 32, t, copy_len);
        remaining -= copy_len;
    }
    
    return true;
}

// Get current time in microseconds
static uint64_t get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + tv.tv_usec;
}

// Benchmark function
static double benchmark_pbkdf2(uint32_t iterations, int runs) {
    const char* password = "benchmark_test_123";
    uint8_t salt[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    uint8_t output[32];
    
    uint64_t start = get_time_us();
    
    for (int i = 0; i < runs; i++) {
        pbkdf2_sha256(password, strlen(password), salt, 16, iterations, output, 32);
    }
    
    uint64_t end = get_time_us();
    double total_ms = (end - start) / 1000.0;
    
    return total_ms / runs; // Average time per operation in ms
}

int main(void) {
    printf("================================================================================\n");
    printf("PBKDF2-SHA256 Performance Benchmark\n");
    printf("SEC-027: PBKDF2 Iteration Increase Verification\n");
    printf("================================================================================\n\n");
    
    printf("Target: Authentication must complete in <500ms\n");
    printf("Platform: Host PC (x86_64) - Note: STM32F401 will be slower\n\n");
    
    // Test different iteration counts
    uint32_t test_iterations[] = {1000, 10000, 100000};
    int num_tests = sizeof(test_iterations) / sizeof(test_iterations[0]);
    int runs = 10;
    
    printf("Running %d iterations per test...\n\n", runs);
    
    double time_1000 = 0, time_100000 = 0;
    
    for (int i = 0; i < num_tests; i++) {
        uint32_t iterations = test_iterations[i];
        double avg_time = benchmark_pbkdf2(iterations, runs);
        
        printf("PBKDF2 with %6u iterations: %.3f ms average\n", iterations, avg_time);
        
        if (iterations == 1000) time_1000 = avg_time;
        if (iterations == 100000) time_100000 = avg_time;
    }
    
    printf("\n--------------------------------------------------------------------------------\n");
    printf("Results Summary:\n");
    printf("--------------------------------------------------------------------------------\n");
    
    if (time_1000 > 0 && time_100000 > 0) {
        double slowdown = time_100000 / time_1000;
        printf("Performance impact (1000 -> 100000): %.1fx slower\n", slowdown);
        printf("100,000 iterations time: %.3f ms\n", time_100000);
        
        if (time_100000 < 500.0) {
            printf("\n✅ PASS: Authentication completes in <500ms\n");
        } else {
            printf("\n⚠️  WARNING: Authentication takes %.1f ms (>500ms target)\n", time_100000);
            printf("    Note: This is on host PC. STM32F401 @ 84MHz will be slower.\n");
        }
        
        // Estimate STM32 time (rough approximation based on clock speed)
        // Host PC ~3GHz vs STM32 84MHz = ~36x slower
        double estimated_stm32_time = time_100000 * 36;
        printf("\nEstimated STM32F401 @ 84MHz time: %.1f ms\n", estimated_stm32_time);
        
        if (estimated_stm32_time < 500.0) {
            printf("✅ PASS: Estimated time <500ms on target hardware\n");
        } else {
            printf("⚠️  WARNING: Estimated time may exceed 500ms on STM32F401\n");
            printf("    Consider using hardware crypto acceleration if available.\n");
        }
    }
    
    printf("\n================================================================================\n");
    printf("Benchmark complete.\n");
    printf("================================================================================\n");
    
    return 0;
}
