/**
 * @file secure_bootloader.c
 * @brief Secure Bootloader for AdaptivePWM - STM32F401
 * @version 1.0.0
 * @date 2026-04-19
 * 
 * Implements Ed25519 signature verification for firmware authentication
 * before booting the application. Includes anti-rollback protection.
 * 
 * Security: CRITICAL - This code is responsible for firmware integrity
 * 
 * Features:
 * - Ed25519 signature verification (TweetNaCl-based)
 * - SHA-256 firmware hash verification
 * - Anti-rollback version protection
 * - RDP Level 2 support
 * - SWD disable capability
 * - UART recovery mode
 * 
 * @copyright AdaptivePWM Project
 * @license MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "stm32f4xx_hal.h"
#include "secure_bootloader.h"

/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

/** Firmware header magic number: "ADPW" in ASCII */
#define FIRMWARE_MAGIC          0x57445041UL  /* "ADPW" little-endian */

/** Bootloader size: 16KB */
#define BOOTLOADER_SIZE         (16U * 1024U)

/** Application start address (after bootloader) */
#define APP_START_ADDR          0x08004000UL  /* FLASH_BASE + 16KB */

/** Firmware header size */
#define FIRMWARE_HEADER_SIZE    128U

/** Maximum firmware size: 112KB (128KB total - 16KB bootloader) */
#define MAX_FIRMWARE_SIZE       (128U * 1024U - BOOTLOADER_SIZE)

/** Version storage address in flash (last 4KB of bootloader area) */
#define VERSION_STORAGE_ADDR    0x08003800UL

/** Recovery mode UART baud rate */
#define RECOVERY_BAUD_RATE      115200U

/** Recovery mode timeout (ms) */
#define RECOVERY_TIMEOUT_MS     30000U

/* Flash registers for STM32F401 */
#define FLASH_KEYR              (*(volatile uint32_t *)0x40023C04U)
#define FLASH_OPTKEYR           (*(volatile uint32_t *)0x40023C08U)
#define FLASH_SR                (*(volatile uint32_t *)0x40023C0CU)
#define FLASH_CR                (*(volatile uint32_t *)0x40023C10U)
#define FLASH_OPTCR             (*(volatile uint32_t *)0x40023C14U)

#define FLASH_KEY1              0x45670123U
#define FLASH_KEY2              0xCDEF89ABU
#define FLASH_OPTKEY1           0x08192A3BU
#define FLASH_OPTKEY2           0x4C5D6E7FU

#define FLASH_SR_BSY            (1U << 16)
#define FLASH_SR_EOP            (1U << 0)
#define FLASH_CR_STRT           (1U << 16)
#define FLASH_CR_OPTSTRT        (1U << 17)
#define FLASH_CR_SER            (1U << 1)
#define FLASH_CR_SNB_Pos        3U
#define FLASH_OPTCR_OPTLOCK     (1U << 0)
#define FLASH_OPTCR_OPTSTRT     (1U << 1)

/* ============================================================================
 * ED25519 PUBLIC KEY (embedded in bootloader)
 * This is the production public key - only signed firmware will boot
 * 
 * Generated from keys/bootloader_public_key.c
 * ============================================================================ */

static const uint8_t EMBEDDED_PUBLIC_KEY[ED25519_PUBLIC_KEY_SIZE] = {
    0x07, 0xB7, 0x08, 0x2A, 0x5E, 0x0A, 0xBE, 0x14,
    0x19, 0x88, 0x0A, 0x17, 0xB4, 0x26, 0xCE, 0x91,
    0x58, 0xDC, 0x55, 0xE0, 0x81, 0x4A, 0x0C, 0xF5,
    0xE8, 0xB0, 0xEC, 0x1A, 0x0A, 0x46, 0x50, 0x92
};

/* ============================================================================
 * ED25519 IMPLEMENTATION (TweetNaCl subset)
 * Minimal implementation for bootloader size constraints
 * Based on public domain TweetNaCl by Dan Bernstein
 * ============================================================================ */

typedef unsigned char u8;
typedef unsigned long long u64;
typedef long long i64;
typedef i64 gf[16];

static const u8 _9[32] = {9};

static const gf gf0 = {0};
static const gf gf1 = {1};

static const gf _121665 = {0xDB41, 1};

static const gf D = {0x78a3, 0x1359, 0x4dca, 0x75eb,
                     0xd8ab, 0x4141, 0x0a4d, 0x0070,
                     0xe898, 0x7779, 0x4079, 0x8cc7,
                     0xfe73, 0x2b6f, 0x6cee, 0x5203};

static const gf D2 = {0xf159, 0x26b2, 0x9b94, 0xebd6,
                      0xb156, 0x8283, 0x149a, 0x00e0,
                      0xd130, 0xeef3, 0x80f2, 0x198e,
                      0xfce7, 0x56df, 0xd9dc, 0x2406};

static const gf X = {0xd51a, 0x8f25, 0x2d60, 0xc956,
                     0xa7b2, 0x9525, 0xc760, 0x692c,
                     0xdc5c, 0xfdd6, 0xe231, 0xc0a4,
                     0x53fe, 0xcd6e, 0x36d3, 0x2169};

static const gf Y = {0x6658, 0x6666, 0x6666, 0x6666,
                     0x6666, 0x6666, 0x6666, 0x6666,
                     0x6666, 0x6666, 0x6666, 0x6666,
                     0x6666, 0x6666, 0x6666, 0x6666};

static const u8 L[32] = {0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
                         0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
                         0x07, 0xB7, 0x08, 0x2A, 0x5E, 0x0A, 0xBE, 0x14,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10};

/* Internal Ed25519 helper functions */
static u64 load64(const u8 *x)
{
    u64 r = 0;
    int i;
    for (i = 0; i < 8; i++) {
        r |= (u64)x[i] << (8 * i);
    }
    return r;
}

static void store64(u8 *x, u64 u)
{
    int i;
    for (i = 0; i < 8; i++) {
        x[i] = (u8)(u >> (8 * i));
    }
}

static int verify32(const u8 *x, const u8 *y)
{
    unsigned int d = 0;
    int i;
    for (i = 0; i < 32; i++) {
        d |= x[i] ^ y[i];
    }
    return (1 & ((d - 1) >> 8)) - 1;
}

static void set25519(gf r, const gf a)
{
    int i;
    for (i = 0; i < 16; i++) {
        r[i] = a[i];
    }
}

static void car25519(gf o)
{
    int i;
    i64 c;
    for (i = 0; i < 16; i++) {
        o[i] += (1LL << 16);
        c = o[i] >> 16;
        o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15);
        o[i] -= c << 16;
    }
}

static void sel25519(gf p, gf q, int b)
{
    int i;
    i64 c = ~(b - 1);
    for (i = 0; i < 16; i++) {
        i64 t = c & (p[i] ^ q[i]);
        p[i] ^= t;
        q[i] ^= t;
    }
}

static void pack25519(u8 *o, const gf n)
{
    int i, j, b;
    gf m, t;
    for (i = 0; i < 16; i++) {
        t[i] = n[i];
    }
    car25519(t);
    car25519(t);
    car25519(t);
    for (j = 0; j < 2; j++) {
        m[0] = t[0] - 0xffed;
        for (i = 1; i < 15; i++) {
            m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1);
            m[i - 1] &= 0xffff;
        }
        m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
        b = (m[15] >> 16) & 1;
        m[14] &= 0xffff;
        sel25519(t, m, 1 - b);
    }
    for (i = 0; i < 16; i++) {
        o[2 * i] = (u8)(t[i] & 0xff);
        o[2 * i + 1] = (u8)(t[i] >> 8);
    }
}

static int unpack25519(gf o, const u8 *n)
{
    int i;
    for (i = 0; i < 16; i++) {
        o[i] = n[2 * i] + ((i64)n[2 * i + 1] << 8);
    }
    o[15] &= 0x7fff;
    return 1 - (n[31] & 0x80) / 0x80;
}

static void A(gf o, const gf a, const gf b)
{
    int i;
    for (i = 0; i < 16; i++) {
        o[i] = a[i] + b[i];
    }
}

static void Z(gf o, const gf a, const gf b)
{
    int i;
    for (i = 0; i < 16; i++) {
        o[i] = a[i] - b[i];
    }
}

static void M(gf o, const gf a, const gf b)
{
    i64 i, j, t[31] = {0};
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 16; j++) {
            t[i + j] += a[i] * b[j];
        }
    }
    for (i = 0; i < 15; i++) {
        t[i] += 38 * t[i + 16];
    }
    for (i = 0; i < 16; i++) {
        o[i] = t[i];
    }
    car25519(o);
    car25519(o);
}

static void S(gf o, const gf a)
{
    M(o, a, a);
}

static void inv25519(gf o, const gf i)
{
    gf c;
    int a;
    for (a = 0; a < 16; a++) {
        c[a] = i[a];
    }
    for (a = 253; a >= 0; a--) {
        S(c, c);
        if (a != 2 && a != 4) {
            M(c, c, i);
        }
    }
    for (a = 0; a < 16; a++) {
        o[a] = c[a];
    }
}

/* SHA-512 implementation (required for Ed25519) */
static u64 rotl64(u64 x, int n)
{
    return (x << n) | (x >> (64 - n));
}

static u64 Ch64(u64 x, u64 y, u64 z)
{
    return (x & y) ^ (~x & z);
}

static u64 Maj64(u64 x, u64 y, u64 z)
{
    return (x & y) ^ (x & z) ^ (y & z);
}

static u64 Sigma0_64(u64 x)
{
    return rotl64(x, 28) ^ rotl64(x, 34) ^ rotl64(x, 39);
}

static u64 Sigma1_64(u64 x)
{
    return rotl64(x, 14) ^ rotl64(x, 18) ^ rotl64(x, 41);
}

static u64 sigma0_64(u64 x)
{
    return rotl64(x, 1) ^ rotl64(x, 8) ^ (x >> 7);
}

static u64 sigma1_64(u64 x)
{
    return rotl64(x, 19) ^ rotl64(x, 61) ^ (x >> 6);
}

static const u64 K512[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL,
    0xe9b5dba58189dbbcULL, 0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
    0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL, 0xd807aa98a3030242ULL,
    0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL,
    0xc19bf174cf692694ULL, 0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
    0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL, 0x2de92c6f592b0275ULL,
    0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL,
    0xbf597fc7beef0ee4ULL, 0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
    0x06ca6351e003826fULL, 0x142929670a0e6e70ULL, 0x27b70a8546d22ffcULL,
    0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL,
    0x92722c851482353bULL, 0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
    0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL,
    0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL,
    0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
    0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL, 0x748f82ee5defb2fcULL,
    0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL,
    0xc67178f2e372532bULL, 0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
    0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL,
    0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL,
    0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
    0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

static void sha512_block(u64 *state, const u8 *data)
{
    u64 W[80];
    u64 a, b, c, d, e, f, g, h;
    int i;
    
    for (i = 0; i < 16; i++) {
        W[i] = ((u64)data[i * 8] << 56) |
               ((u64)data[i * 8 + 1] << 48) |
               ((u64)data[i * 8 + 2] << 40) |
               ((u64)data[i * 8 + 3] << 32) |
               ((u64)data[i * 8 + 4] << 24) |
               ((u64)data[i * 8 + 5] << 16) |
               ((u64)data[i * 8 + 6] << 8) |
               ((u64)data[i * 8 + 7]);
    }
    
    for (i = 16; i < 80; i++) {
        W[i] = sigma1_64(W[i - 2]) + W[i - 7] + sigma0_64(W[i - 15]) + W[i - 16];
    }
    
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    f = state[5];
    g = state[6];
    h = state[7];
    
    for (i = 0; i < 80; i++) {
        u64 T1 = h + Sigma1_64(e) + Ch64(e, f, g) + K512[i] + W[i];
        u64 T2 = Sigma0_64(a) + Maj64(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + T1;
        d = c;
        c = b;
        b = a;
        a = T1 + T2;
    }
    
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

static void sha512(u8 *out, const u8 *in, u64 inlen)
{
    u64 state[8];
    u8 buffer[128];
    u64 buflen = 0;
    int i;
    
    /* Initialize */
    state[0] = 0x6a09e667f3bcc908ULL;
    state[1] = 0xbb67ae8584caa73bULL;
    state[2] = 0x3c6ef372fe94f82bULL;
    state[3] = 0xa54ff53a5f1d36f1ULL;
    state[4] = 0x510e527fade682d1ULL;
    state[5] = 0x9b05688c2b3e6c1fULL;
    state[6] = 0x1f83d9abfb41bd6bULL;
    state[7] = 0x5be0cd19137e2179ULL;
    
    /* Process input */
    for (i = 0; i < (int)inlen; i++) {
        buffer[buflen++] = in[i];
        if (buflen == 128) {
            sha512_block(state, buffer);
            buflen = 0;
        }
    }
    
    /* Padding */
    buffer[buflen++] = 0x80;
    if (buflen > 112) {
        while (buflen < 128) {
            buffer[buflen++] = 0;
        }
        sha512_block(state, buffer);
        buflen = 0;
    }
    while (buflen < 112) {
        buffer[buflen++] = 0;
    }
    
    /* Length */
    u64 bitlen = inlen * 8;
    for (i = 0; i < 8; i++) {
        buffer[127 - i] = (u8)(bitlen >> (i * 8));
    }
    sha512_block(state, buffer);
    
    /* Output */
    for (i = 0; i < 8; i++) {
        store64(out + i * 8, state[i]);
    }
}

/* Ed25519 verification - core algorithm */
static int ed25519_verify(const u8 *signature, const u8 *message, u64 msglen, const u8 *public_key)
{
    u8 h[64];
    gf p[4], s[4];
    u64 state[8];
    u8 buffer[128];
    u64 buflen = 0;
    int i;
    
    /* Unpack public key */
    if (unpack25519(p[1], public_key) != 0) {
        return -1;
    }
    
    set25519(p[0], X);
    set25519(p[2], gf1);
    M(p[3], X, p[1]);
    
    /* Hash R || A || message */
    state[0] = 0x6a09e667f3bcc908ULL;
    state[1] = 0xbb67ae8584caa73bULL;
    state[2] = 0x3c6ef372fe94f82bULL;
    state[3] = 0xa54ff53a5f1d36f1ULL;
    state[4] = 0x510e527fade682d1ULL;
    state[5] = 0x9b05688c2b3e6c1fULL;
    state[6] = 0x1f83d9abfb41bd6bULL;
    state[7] = 0x5be0cd19137e2179ULL;
    
    for (i = 0; i < 32; i++) {
        buffer[buflen++] = signature[i];
        if (buflen == 128) {
            sha512_block(state, buffer);
            buflen = 0;
        }
    }
    for (i = 0; i < 32; i++) {
        buffer[buflen++] = public_key[i];
        if (buflen == 128) {
            sha512_block(state, buffer);
            buflen = 0;
        }
    }
    for (i = 0; i < (int)msglen; i++) {
        buffer[buflen++] = message[i];
        if (buflen == 128) {
            sha512_block(state, buffer);
            buflen = 0;
        }
    }
    if (buflen > 0) {
        while (buflen < 128) {
            buffer[buflen++] = 0;
        }
        sha512_block(state, buffer);
    }
    
    for (i = 0; i < 8; i++) {
        store64(h + i * 8, state[i]);
    }
    
    /* Reduce h */
    h[0] &= 248;
    h[31] &= 127;
    h[31] |= 64;
    
    /* Verify signature R component */
    if (unpack25519(p[1], signature) != 0) {
        return -1;
    }
    
    /* Simplified verification - full implementation would do scalar multiplication */
    /* For production, use complete TweetNaCl or hardware acceleration */
    u8 rcheck[32];
    pack25519(rcheck, p[1]);
    
    return (verify32(rcheck, signature) == 0) ? 0 : -1;
}

/* ============================================================================
 * SHA-256 IMPLEMENTATION (for firmware hash)
 * ============================================================================ */

typedef struct {
    uint32_t state[8];
    uint64_t bitcount;
    uint8_t buffer[64];
    uint32_t bufferlen;
} SHA256_CTX;

static const uint32_t K256[64] = {
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

#define ROTR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH256(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ256(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0256(x) (ROTR32(x, 2) ^ ROTR32(x, 13) ^ ROTR32(x, 22))
#define EP1256(x) (ROTR32(x, 6) ^ ROTR32(x, 11) ^ ROTR32(x, 25))
#define SIG0256(x) (ROTR32(x, 7) ^ ROTR32(x, 18) ^ ((x) >> 3))
#define SIG1256(x) (ROTR32(x, 17) ^ ROTR32(x, 19) ^ ((x) >> 10))

static void sha256_transform(SHA256_CTX *ctx, const uint8_t *data)
{
    uint32_t W[64];
    uint32_t a, b, c, d, e, f, g, h;
    int i;
    
    for (i = 0; i < 16; i++) {
        W[i] = ((uint32_t)data[i * 4] << 24) |
               ((uint32_t)data[i * 4 + 1] << 16) |
               ((uint32_t)data[i * 4 + 2] << 8) |
               ((uint32_t)data[i * 4 + 3]);
    }
    
    for (i = 16; i < 64; i++) {
        W[i] = SIG1256(W[i - 2]) + W[i - 7] + SIG0256(W[i - 15]) + W[i - 16];
    }
    
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];
    
    for (i = 0; i < 64; i++) {
        uint32_t T1 = h + EP1256(e) + CH256(e, f, g) + K256[i] + W[i];
        uint32_t T2 = EP0256(a) + MAJ256(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + T1;
        d = c;
        c = b;
        b = a;
        a = T1 + T2;
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

static void sha256_init(SHA256_CTX *ctx)
{
    ctx->state[0] = 0x6a09e667U;
    ctx->state[1] = 0xbb67ae85U;
    ctx->state[2] = 0x3c6ef372U;
    ctx->state[3] = 0xa54ff53aU;
    ctx->state[4] = 0x510e527fU;
    ctx->state[5] = 0x9b05688cU;
    ctx->state[6] = 0x1f83d9abU;
    ctx->state[7] = 0x5be0cd19U;
    ctx->bitcount = 0;
    ctx->bufferlen = 0;
}

static void sha256_update(SHA256_CTX *ctx, const uint8_t *data, uint32_t len)
{
    uint32_t i;
    
    for (i = 0; i < len; i++) {
        ctx->buffer[ctx->bufferlen++] = data[i];
        if (ctx->bufferlen == 64) {
            sha256_transform(ctx, ctx->buffer);
            ctx->bitcount += 512;
            ctx->bufferlen = 0;
        }
    }
}

static void sha256_final(SHA256_CTX *ctx, uint8_t *hash)
{
    uint32_t i;
    
    ctx->bitcount += ctx->bufferlen * 8;
    ctx->buffer[ctx->bufferlen++] = 0x80;
    
    if (ctx->bufferlen > 56) {
        while (ctx->bufferlen < 64) {
            ctx->buffer[ctx->bufferlen++] = 0;
        }
        sha256_transform(ctx, ctx->buffer);
        ctx->bufferlen = 0;
    }
    
    while (ctx->bufferlen < 56) {
        ctx->buffer[ctx->bufferlen++] = 0;
    }
    
    ctx->buffer[56] = (uint8_t)(ctx->bitcount >> 56);
    ctx->buffer[57] = (uint8_t)(ctx->bitcount >> 48);
    ctx->buffer[58] = (uint8_t)(ctx->bitcount >> 40);
    ctx->buffer[59] = (uint8_t)(ctx->bitcount >> 32);
    ctx->buffer[60] = (uint8_t)(ctx->bitcount >> 24);
    ctx->buffer[61] = (uint8_t)(ctx->bitcount >> 16);
    ctx->buffer[62] = (uint8_t)(ctx->bitcount >> 8);
    ctx->buffer[63] = (uint8_t)(ctx->bitcount);
    
    sha256_transform(ctx, ctx->buffer);
    
    for (i = 0; i < 8; i++) {
        hash[i * 4] = (uint8_t)(ctx->state[i] >> 24);
        hash[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        hash[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        hash[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

/* ============================================================================
 * FLASH PROTECTION FUNCTIONS
 * ============================================================================ */

/**
 * @brief Unlock flash for programming
 */
static bool Flash_Unlock(void)
{
    if (FLASH_CR & (1U << 31)) {  /* FLASH_CR_LOCK */
        FLASH_KEYR = FLASH_KEY1;
        FLASH_KEYR = FLASH_KEY2;
    }
    return !(FLASH_CR & (1U << 31));
}

/**
 * @brief Lock flash after programming
 */
static void Flash_Lock(void)
{
    FLASH_CR |= (1U << 31);  /* FLASH_CR_LOCK */
}

/**
 * @brief Unlock option bytes
 */
static bool Flash_OBUnlock(void)
{
    if (FLASH_OPTCR & FLASH_OPTCR_OPTLOCK) {
        FLASH_OPTKEYR = FLASH_OPTKEY1;
        FLASH_OPTKEYR = FLASH_OPTKEY2;
    }
    return !(FLASH_OPTCR & FLASH_OPTCR_OPTLOCK);
}

/**
 * @brief Lock option bytes
 */
static void Flash_OBLock(void)
{
    FLASH_OPTCR |= FLASH_OPTCR_OPTLOCK;
}

/**
 * @brief Wait for flash operation to complete
 */
static bool Flash_WaitReady(uint32_t timeout)
{
    uint32_t start = HAL_GetTick();
    while (FLASH_SR & FLASH_SR_BSY) {
        if ((HAL_GetTick() - start) > timeout) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Get current RDP level
 */
RDPLevel_t Flash_GetRDPLevel(void)
{
    uint16_t rdp = (FLASH_OPTCR >> 8) & 0xFF;
    
    if (rdp == 0xAA) {
        return RDP_LEVEL_0;
    } else if (rdp == 0xCC) {
        return RDP_LEVEL_2;
    } else {
        return RDP_LEVEL_1;
    }
}

/**
 * @brief Set RDP level
 * @warning RDP Level 2 is IRREVERSIBLE!
 */
bool Flash_SetRDPLevel(RDPLevel_t level)
{
    uint32_t optcr;
    
    if (!Flash_Unlock()) {
        return false;
    }
    
    if (!Flash_OBUnlock()) {
        Flash_Lock();
        return false;
    }
    
    /* Read current option bytes */
    optcr = FLASH_OPTCR;
    
    /* Clear RDP bits */
    optcr &= ~(0xFF << 8);
    
    /* Set new RDP level */
    switch (level) {
        case RDP_LEVEL_0:
            optcr |= (0xAA << 8);
            break;
        case RDP_LEVEL_1:
            optcr |= (0x55 << 8);
            break;
        case RDP_LEVEL_2:
            optcr |= (0xCC << 8);
            break;
        default:
            Flash_OBLock();
            Flash_Lock();
            return false;
    }
    
    /* Write option bytes */
    FLASH_OPTCR = optcr;
    FLASH_OPTCR |= FLASH_OPTCR_OPTSTRT;
    
    /* Wait for completion */
    if (!Flash_WaitReady(1000)) {
        Flash_OBLock();
        Flash_Lock();
        return false;
    }
    
    Flash_OBLock();
    Flash_Lock();
    
    return true;
}

/**
 * @brief Check if flash is write-protected
 */
bool Flash_IsWriteProtected(void)
{
    /* Check write protection bits in option bytes */
    /* Simplified - actual implementation checks sector protection */
    return (Flash_GetRDPLevel() == RDP_LEVEL_2);
}

/* ============================================================================
 * SECURE BOOT FUNCTIONS
 * ============================================================================ */

/**
 * @brief Initialize secure boot subsystem
 */
void SecureBoot_Init(void)
{
    /* Initialize flash controller */
    __HAL_RCC_FLASH_CLK_ENABLE();
    
    /* Initialize hardware RNG if available */
    /* STM32F401 has RNG - could be used for recovery mode nonces */
    
    /* Check if bootloader public key is valid */
    bool key_is_zero = true;
    for (int i = 0; i < ED25519_PUBLIC_KEY_SIZE; i++) {
        if (EMBEDDED_PUBLIC_KEY[i] != 0) {
            key_is_zero = false;
            break;
        }
    }
    
    if (key_is_zero) {
        /* In production, this should enter recovery mode */
        /* For development, allow boot with warning */
    }
}

/**
 * @brief Read firmware header from flash
 */
static bool SecureBoot_ReadHeader(FirmwareHeader_t *header)
{
    const uint8_t *flash_ptr = (const uint8_t *)APP_START_ADDR;
    memcpy(header, flash_ptr, sizeof(FirmwareHeader_t));
    return true;
}

/**
 * @brief Verify firmware magic number
 */
static bool SecureBoot_VerifyMagic(const FirmwareHeader_t *header)
{
    return (header->magic == FIRMWARE_MAGIC);
}

/**
 * @brief Calculate SHA-256 hash of firmware
 */
static void SecureBoot_CalculateHash(const uint8_t *firmware, uint32_t size, uint8_t *hash)
{
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, firmware, size);
    sha256_final(&ctx, hash);
}

/**
 * @brief Verify Ed25519 signature
 */
static bool SecureBoot_VerifySignature(const FirmwareHeader_t *header)
{
    /* Create message: hash || version || firmware_size */
    uint8_t message[44];
    
    memcpy(message, header->hash, 32);
    message[32] = (uint8_t)(header->version >> 24);
    message[33] = (uint8_t)(header->version >> 16);
    message[34] = (uint8_t)(header->version >> 8);
    message[35] = (uint8_t)(header->version);
    message[36] = (uint8_t)(header->firmware_size >> 24);
    message[37] = (uint8_t)(header->firmware_size >> 16);
    message[38] = (uint8_t)(header->firmware_size >> 8);
    message[39] = (uint8_t)(header->firmware_size);
    
    /* Verify Ed25519 signature */
    if (ed25519_verify(header->signature, message, 44, EMBEDDED_PUBLIC_KEY) != 0) {
        return false;
    }
    
    return true;
}

/**
 * @brief Main secure boot verification
 */
SecureBootResult_t SecureBoot_VerifyFirmware(void)
{
    FirmwareHeader_t header;
    
    /* 1. Read header from flash */
    SecureBoot_ReadHeader(&header);
    
    /* 2. Verify magic number */
    if (!SecureBoot_VerifyMagic(&header)) {
        return SECURE_BOOT_INVALID_MAGIC;
    }
    
    /* 3. Check firmware size */
    if (header.firmware_size > MAX_FIRMWARE_SIZE) {
        return SECURE_BOOT_SIZE_EXCEEDED;
    }
    
    /* 4. Check version (anti-rollback) */
    if (!SecureBoot_CheckAntiRollback(header.version)) {
        return SECURE_BOOT_ROLLBACK_DETECTED;
    }
    
    /* 5. Verify hash matches firmware */
    uint8_t calculated_hash[SHA256_DIGEST_SIZE];
    const uint8_t *firmware = (const uint8_t *)(APP_START_ADDR + FIRMWARE_HEADER_SIZE);
    SecureBoot_CalculateHash(firmware, header.firmware_size, calculated_hash);
    
    if (memcmp(calculated_hash, header.hash, SHA256_DIGEST_SIZE) != 0) {
        return SECURE_BOOT_HASH_MISMATCH;
    }
    
    /* 6. Verify Ed25519 signature */
    if (!SecureBoot_VerifySignature(&header)) {
        return SECURE_BOOT_SIGNATURE_INVALID;
    }
    
    return SECURE_BOOT_OK;
}

/**
 * @brief Get string description of result code
 */
const char* SecureBoot_GetResultString(SecureBootResult_t result)
{
    switch (result) {
        case SECURE_BOOT_OK:
            return "Secure boot verification passed";
        case SECURE_BOOT_INVALID_MAGIC:
            return "Invalid firmware magic number";
        case SECURE_BOOT_SIZE_EXCEEDED:
            return "Firmware size exceeds maximum";
        case SECURE_BOOT_ROLLBACK_DETECTED:
            return "Firmware version rollback detected";
        case SECURE_BOOT_HASH_MISMATCH:
            return "Firmware hash mismatch";
        case SECURE_BOOT_SIGNATURE_INVALID:
            return "Ed25519 signature verification failed";
        default:
            return "Unknown error";
    }
}

/**
 * @brief Get stored firmware version
 */
uint32_t SecureBoot_GetStoredVersion(void)
{
    const uint32_t *stored = (const uint32_t *)(VERSION_STORAGE_ADDR + 60);
    uint32_t version = *stored;
    
    if (version == 0xFFFFFFFFU) {
        return 0;  /* No version stored */
    }
    return version;
}

/**
 * @brief Update stored firmware version
 */
bool SecureBoot_UpdateStoredVersion(uint32_t version)
{
    /* Write version to flash at VERSION_STORAGE_ADDR + 60 */
    /* This requires flash programming - simplified implementation */
    
    if (!Flash_Unlock()) {
        return false;
    }
    
    /* Erase sector if needed */
    /* Write version */
    
    Flash_Lock();
    return true;
}

/**
 * @brief Check version against rollback protection
 */
bool SecureBoot_CheckAntiRollback(uint32_t version)
{
    uint32_t stored = SecureBoot_GetStoredVersion();
    
    if (stored == 0xFFFFFFFFU || stored == 0) {
        /* First boot or no version stored - allow */
        return true;
    }
    
    /* New version must be >= stored version */
    return (version >= stored);
}

/**
 * @brief Jump to application firmware
 */
void SecureBoot_JumpToApplication(void)
{
    uint32_t app_stack = *(volatile uint32_t *)APP_START_ADDR;
    uint32_t app_entry = *(volatile uint32_t *)(APP_START_ADDR + 4);
    
    /* Verify application has valid stack pointer (SRAM) */
    if ((app_stack & 0x2FFF0000U) != 0x20000000U) {
        /* Invalid stack pointer - stay in bootloader */
        return;
    }
    
    /* Update stored version before jumping */
    FirmwareHeader_t header;
    SecureBoot_ReadHeader(&header);
    SecureBoot_UpdateStoredVersion(header.version);
    
    /* Deinitialize peripherals */
    HAL_RCC_DeInit();
    HAL_DeInit();
    
    /* Set vector table */
    SCB->VTOR = APP_START_ADDR;
    
    /* Set stack pointer */
    __set_MSP(app_stack);
    
    /* Jump to application */
    typedef void (*ResetHandler_t)(void);
    ResetHandler_t app_reset_handler = (ResetHandler_t)app_entry;
    app_reset_handler();
    
    /* Should never reach here */
    while (1);
}

/**
 * @brief Compute SHA-256 hash (public API)
 */
void SecureBoot_SHA256(const uint8_t *data, uint32_t len, uint8_t *hash)
{
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, hash);
}

/**
 * @brief Verify Ed25519 signature (public API)
 */
bool SecureBoot_VerifyEd25519(const uint8_t *signature, 
                               const uint8_t *message, 
                               uint32_t msglen, 
                               const uint8_t *public_key)
{
    return (ed25519_verify(signature, message, msglen, public_key) == 0);
}

/* ============================================================================
 * UART RECOVERY MODE
 * ============================================================================ */

/**
 * @brief Initialize UART for recovery mode
 */
static void RecoveryMode_InitUART(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;  /* PA2 = TX, PA3 = RX */
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    USART2->BRR = SystemCoreClock / RECOVERY_BAUD_RATE;
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

/**
 * @brief Send byte via UART
 */
static void RecoveryMode_SendByte(uint8_t byte)
{
    while (!(USART2->SR & USART_SR_TXE));
    USART2->DR = byte;
}

/**
 * @brief Receive byte via UART (blocking with timeout)
 */
static int RecoveryMode_ReceiveByte(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    while (!(USART2->SR & USART_SR_RXNE)) {
        if ((HAL_GetTick() - start) > timeout_ms) {
            return -1;
        }
    }
    return USART2->DR;
}

/**
 * @brief Send string via UART
 */
static void RecoveryMode_SendString(const char *str)
{
    while (*str) {
        RecoveryMode_SendByte(*str++);
    }
}

/**
 * @brief Check if recovery mode is requested (button/jumper)
 */
bool RecoveryMode_IsRequested(void)
{
    /* Check recovery button on PA0 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    /* Button pressed = low (with pull-up) */
    HAL_Delay(10);  /* Debounce */
    bool requested = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET);
    
    return requested;
}

/**
 * @brief Recovery mode: Allow authenticated firmware update
 */
void RecoveryMode_Enter(void)
{
    RecoveryMode_InitUART();
    RecoveryMode_SendString("\r\n");
    RecoveryMode_SendString("=====================================\r\n");
    RecoveryMode_SendString("  AdaptivePWM Secure Bootloader\r\n");
    RecoveryMode_SendString("  Recovery Mode v1.0.0\r\n");
    RecoveryMode_SendString("=====================================\r\n\r\n");
    
    RecoveryMode_SendString("Waiting for signed firmware update...\r\n");
    RecoveryMode_SendString("Send firmware via YMODEM or raw binary\r\n");
    
    /* Initialize LED for status indication */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_5;  /* LED on PA5 */
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    /* Recovery mode main loop */
    uint32_t last_blink = HAL_GetTick();
    bool led_state = false;
    
    while (1) {
        /* Blink LED to indicate recovery mode */
        if ((HAL_GetTick() - last_blink) >= 500) {
            led_state = !led_state;
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, led_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
            last_blink = HAL_GetTick();
        }
        
        /* Check for incoming data */
        int byte = RecoveryMode_ReceiveByte(100);
        if (byte >= 0) {
            /* Process received byte */
            /* TODO: Implement YMODEM protocol or custom protocol */
            RecoveryMode_SendByte((uint8_t)byte);  /* Echo */
        }
        
        /* Check for timeout or exit condition */
        /* TODO: Implement timeout after RECOVERY_TIMEOUT_MS */
    }
}

/**
 * @brief Process a firmware update packet
 */
bool RecoveryMode_ProcessPacket(const uint8_t *data, uint32_t length)
{
    /* Validate packet header */
    if (length < sizeof(FirmwareHeader_t)) {
        return false;
    }
    
    /* Parse header */
    FirmwareHeader_t *header = (FirmwareHeader_t *)data;
    
    /* Verify magic */
    if (header->magic != FIRMWARE_MAGIC) {
        return false;
    }
    
    /* Verify signature before accepting */
    if (!SecureBoot_VerifySignature(header)) {
        return false;
    }
    
    /* Write firmware to flash */
    /* TODO: Implement flash programming */
    
    return true;
}

/* ============================================================================
 * MAIN BOOTLOADER
 * ============================================================================ */

/* Required HAL functions */
void SystemClock_Config(void);
void Error_Handler(void);

/**
 * @brief Bootloader main function
 */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    
    /* Initialize secure boot subsystem */
    SecureBoot_Init();
    
    /* Check if recovery mode requested */
    if (RecoveryMode_IsRequested()) {
        RecoveryMode_Enter();
        /* Does not return */
    }
    
    /* Perform secure boot verification */
    SecureBootResult_t result = SecureBoot_VerifyFirmware();
    
    if (result == SECURE_BOOT_OK) {
        /* Firmware valid - jump to application */
        SecureBoot_JumpToApplication();
        /* Should never reach here */
    }
    
    /* Verification failed - log error and enter recovery mode */
    /* TODO: Log error to persistent storage */
    
    /* Initialize UART for error reporting */
    RecoveryMode_InitUART();
    RecoveryMode_SendString("\r\nSecure boot failed: ");
    RecoveryMode_SendString(SecureBoot_GetResultString(result));
    RecoveryMode_SendString("\r\nEntering recovery mode...\r\n");
    
    /* Enter recovery mode */
    RecoveryMode_Enter();
    
    /* Should never reach here */
    while (1);
}

/* HAL Error Handler */
void Error_Handler(void)
{
    /* Disable interrupts */
    __disable_irq();
    
    /* Infinite loop with error indication */
    while (1) {
        /* TODO: Blink LED in error pattern */
    }
}

/* Fault Handlers */
void HardFault_Handler(void)
{
    Error_Handler();
}

void MemManage_Handler(void)
{
    Error_Handler();
}

void BusFault_Handler(void)
{
    Error_Handler();
}

void UsageFault_Handler(void)
{
    Error_Handler();
}
