/**
 * @file flash_logger_hmac.c
 * @brief HMAC-SHA256 integrity protection implementation
 *
 * Implements HMAC-SHA256 signing and verification for flash logger entries.
 * Uses STM32 HAL for flash operations and mbedTLS for HMAC-SHA256.
 *
 * Security features:
 * - Per-entry random salt (prevents replay attacks)
 * - Chain hashing (prevents deletion/reordering)
 * - Secure key storage abstraction
 * - Comprehensive verification API
 *
 * @version 1.0.0
 * @date 2026-04-12
 */

#include "flash_logger_hmac.h"
#include "config.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>

// mbedTLS headers for HMAC-SHA256
#include "mbedtls/md.h"
#include "mbedtls/sha256.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

// =============================================================================
// CONFIGURATION
// =============================================================================

// Key storage location - Using separate flash sector
// In production, this should be in secure storage (HSM, secure element, or option bytes)
#define HMAC_KEY_FLASH_ADDR    0x080D0000  // Sector 6 (128KB, just before log sector)
#define HMAC_KEY_SECTOR        FLASH_SECTOR_6

// Log entry size with HMAC
#define FLASH_HMAC_ENTRY_SIZE   80
#define FLASH_HMAC_HEADER_SIZE  sizeof(FlashHeaderHMAC_t)

// =============================================================================
// STATIC VARIABLES
// =============================================================================

static HMAC_KeyStorage_t g_hmac_key = { .initialized = false };
static mbedtls_entropy_context g_entropy;
static mbedtls_ctr_drbg_context g_ctr_drbg;
static bool g_rng_initialized = false;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static bool GenerateRandomSalt(uint8_t* salt, size_t len);
static bool EraseKeySector(void);
static bool LoadOrGenerateKey(void);
static bool StoreKeySecurely(const uint8_t* key);

// =============================================================================
// KEY MANAGEMENT IMPLEMENTATION
// =============================================================================

/**
 * @brief Initialize random number generator
 */
static bool InitRNG(void)
{
    if (g_rng_initialized) return true;

    mbedtls_entropy_init(&g_entropy);
    mbedtls_ctr_drbg_init(&g_ctr_drbg);

    const char* pers = "flash_logger_hmac";
    int ret = mbedtls_ctr_drbg_seed(&g_ctr_drbg, mbedtls_entropy_func,
                                    &g_entropy,
                                    (const unsigned char*)pers,
                                    strlen(pers));
    if (ret != 0) {
        mbedtls_ctr_drbg_free(&g_ctr_drbg);
        mbedtls_entropy_free(&g_entropy);
        return false;
    }

    g_rng_initialized = true;
    return true;
}

/**
 * @brief Generate random salt for entry signing
 */
static bool GenerateRandomSalt(uint8_t* salt, size_t len)
{
    if (!g_rng_initialized) {
        if (!InitRNG()) return false;
    }

    int ret = mbedtls_ctr_drbg_random(&g_ctr_drbg, salt, len);
    return ret == 0;
}

/**
 * @brief Erase the key storage sector
 */
static bool EraseKeySector(void)
{
    FLASH_EraseInitTypeDef erase = { 0 };
    uint32_t error = 0;

    HAL_FLASH_Unlock();

    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = HMAC_KEY_SECTOR;
    erase.NbSectors = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    bool result = HAL_FLASHEx_Erase(&erase, &error) == HAL_OK;

    HAL_FLASH_Lock();
    return result;
}

/**
 * @brief Store key in secure flash location
 */
static bool StoreKeySecurely(const uint8_t* key)
{
    HAL_FLASH_Unlock();

    // Write key as 8 x 32-bit words
    bool success = true;
    for (int i = 0; i < HMAC_SHA256_KEY_SIZE / 4; i++) {
        uint32_t word = ((uint32_t*)key)[i];
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                              HMAC_KEY_FLASH_ADDR + i * 4,
                              word) != HAL_OK) {
            success = false;
            break;
        }
    }

    // Write magic and version markers
    if (success) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                          HMAC_KEY_FLASH_ADDR + HMAC_SHA256_KEY_SIZE,
                          FLASH_LOG_HMAC_MAGIC);
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                          HMAC_KEY_FLASH_ADDR + HMAC_SHA256_KEY_SIZE + 4,
                          1); // Version 1
    }

    HAL_FLASH_Lock();
    return success;
}

/**
 * @brief Load existing key or generate new one
 */
static bool LoadOrGenerateKey(void)
{
    // Check if key already exists
    uint32_t* magic_addr = (uint32_t*)(HMAC_KEY_FLASH_ADDR + HMAC_SHA256_KEY_SIZE);

    if (*magic_addr == FLASH_LOG_HMAC_MAGIC) {
        // Load existing key
        memcpy(g_hmac_key.key, (void*)HMAC_KEY_FLASH_ADDR, HMAC_SHA256_KEY_SIZE);
        g_hmac_key.initialized = true;
        return true;
    }

    // Generate new key
    if (!InitRNG()) return false;

    if (mbedtls_ctr_drbg_random(&g_ctr_drbg, g_hmac_key.key, HMAC_SHA256_KEY_SIZE) != 0) {
        return false;
    }

    // Store key securely
    if (!EraseKeySector()) return false;
    if (!StoreKeySecurely(g_hmac_key.key)) return false;

    g_hmac_key.initialized = true;
    return true;
}

bool HMAC_InitKeyStorage(void)
{
    if (g_hmac_key.initialized) return true;
    return LoadOrGenerateKey();
}

const HMAC_KeyStorage_t* HMAC_GetKeyStorage(void)
{
    if (!g_hmac_key.initialized) {
        if (!HMAC_InitKeyStorage()) return NULL;
    }
    return &g_hmac_key;
}

void HMAC_ClearKey(void)
{
    // Secure zeroing
    volatile uint8_t* key = g_hmac_key.key;
    for (int i = 0; i < HMAC_SHA256_KEY_SIZE; i++) {
        key[i] = 0;
    }
    g_hmac_key.initialized = false;
}

// =============================================================================
// HMAC-SHA256 OPERATIONS
// =============================================================================

bool HMAC_ComputeSignature(const LogEntryHMAC_t* entry, uint8_t* signature)
{
    if (!entry || !signature) return false;

    const HMAC_KeyStorage_t* key_storage = HMAC_GetKeyStorage();
    if (!key_storage || !key_storage->initialized) return false;

    // Prepare data for HMAC computation (everything except signature field)
    // Data includes: timestamp, duty_cycle, efficiency, temperature, current,
    // error_code, reserved, salt, prev_hash
    size_t data_len = sizeof(LogEntryHMAC_t) - HMAC_SHA256_SIGNATURE_SIZE;

    // Compute HMAC-SHA256
    int ret = mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                              key_storage->key, HMAC_SHA256_KEY_SIZE,
                              (const uint8_t*)entry, data_len,
                              signature);

    return ret == 0;
}

bool HMAC_VerifySignature(const LogEntryHMAC_t* entry)
{
    if (!entry) return false;

    uint8_t computed_sig[HMAC_SHA256_SIGNATURE_SIZE];
    if (!HMAC_ComputeSignature(entry, computed_sig)) return false;

    // Constant-time comparison to prevent timing attacks
    volatile int result = 0;
    for (int i = 0; i < HMAC_SHA256_SIGNATURE_SIZE; i++) {
        result |= entry->signature[i] ^ computed_sig[i];
    }

    return result == 0;
}

void HMAC_UpdateChainHash(const LogEntryHMAC_t* entry, uint8_t* chain_hash)
{
    if (!entry || !chain_hash) return;

    // Create buffer with previous chain hash + entry signature
    uint8_t hash_input[HMAC_SHA256_SIGNATURE_SIZE + 32];
    memcpy(hash_input, chain_hash, 32);
    memcpy(hash_input + 32, entry->signature, HMAC_SHA256_SIGNATURE_SIZE);

    // Compute new chain hash using SHA256
    mbedtls_sha256(hash_input, sizeof(hash_input), chain_hash, 0);
}

// =============================================================================
// FLASH OPERATIONS
// =============================================================================

static bool EraseLogSector(void)
{
    FLASH_EraseInitTypeDef erase = { 0 };
    uint32_t error = 0;

    HAL_FLASH_Unlock();

    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = FLASH_LOG_SECTOR;
    erase.NbSectors = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    bool result = HAL_FLASHEx_Erase(&erase, &error) == HAL_OK;

    HAL_FLASH_Lock();
    return result;
}

static uint32_t GetNextWriteAddress(FlashLogger_t* logger)
{
    uint32_t base = FLASH_LOG_START_ADDR + sizeof(FlashHeaderHMAC_t);
    uint32_t max_entries = (FLASH_LOG_SIZE - sizeof(FlashHeaderHMAC_t)) / FLASH_HMAC_ENTRY_SIZE;

    // Calculate position based on entry count and wrap count
    uint32_t offset = (logger->entry_count + logger->wrap_count) % max_entries;

    return base + (offset * FLASH_HMAC_ENTRY_SIZE);
}

static bool WriteFlashHeader(const FlashHeaderHMAC_t* header)
{
    HAL_FLASH_Unlock();

    // Write header word by word
    const uint32_t* data = (const uint32_t*)header;
    size_t num_words = sizeof(FlashHeaderHMAC_t) / 4;
    bool success = true;

    for (size_t i = 0; i < num_words; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                              FLASH_LOG_START_ADDR + i * 4,
                              data[i]) != HAL_OK) {
            success = false;
            break;
        }
    }

    HAL_FLASH_Lock();
    return success;
}

static bool WriteLogEntry(uint32_t addr, const LogEntryHMAC_t* entry)
{
    HAL_FLASH_Unlock();

    // Write entry word by word
    const uint32_t* data = (const uint32_t*)entry;
    size_t num_words = sizeof(LogEntryHMAC_t) / 4;
    bool success = true;

    for (size_t i = 0; i < num_words; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                              addr + i * 4,
                              data[i]) != HAL_OK) {
            success = false;
            break;
        }
    }

    HAL_FLASH_Lock();
    return success;
}

// =============================================================================
// FLASH LOGGER HMAC API IMPLEMENTATION
// =============================================================================

bool FlashLoggerHMAC_Init(FlashLogger_t* logger)
{
    if (logger == NULL) return false;

    memset(logger, 0, sizeof(FlashLogger_t));

    // Initialize HMAC key storage
    if (!HMAC_InitKeyStorage()) return false;

    // Check for existing HMAC log
    FlashHeaderHMAC_t* header = (FlashHeaderHMAC_t*)FLASH_LOG_START_ADDR;

    if (header->magic == FLASH_LOG_HMAC_MAGIC) {
        // HMAC log exists
        logger->write_index = header->write_index;
        logger->entry_count = header->entry_count;
        logger->wrap_count = header->wrap_count;
        logger->initialized = true;
    } else if (header->magic == FLASH_MAGIC) {
        // Legacy log exists - will migrate on first write
        logger->write_index = sizeof(FlashHeader_t);
        logger->entry_count = header->entry_count & 0x00FFFFFF; // Mask out version bits
        logger->wrap_count = 0;
        logger->initialized = true;
    } else {
        // Initialize new log
        logger->write_index = sizeof(FlashHeaderHMAC_t);
        logger->entry_count = 0;
        logger->wrap_count = 0;
        logger->initialized = true;

        // Write fresh HMAC header
        FlashHeaderHMAC_t new_header = { 0 };
        new_header.magic = FLASH_LOG_HMAC_MAGIC;
        new_header.version = 1;
        new_header.write_index = logger->write_index;
        new_header.entry_count = 0;
        new_header.wrap_count = 0;

        // Initialize chain hash with zeros
        memset(new_header.chain_hash, 0, 32);

        EraseLogSector();
        WriteFlashHeader(&new_header);
    }

    return true;
}

bool FlashLogger_ConvertToHMAC(const LogEntry_t* legacy,
                               LogEntryHMAC_t* hmac,
                               uint8_t* chain_hash)
{
    if (!legacy || !hmac) return false;

    // Initialize RNG if needed
    if (!g_rng_initialized) {
        if (!InitRNG()) return false;
    }

    // Copy legacy data
    hmac->timestamp = legacy->timestamp;
    hmac->duty_cycle = legacy->duty_cycle;
    hmac->efficiency = legacy->efficiency;
    hmac->temperature = legacy->temperature;
    hmac->current = legacy->current;
    hmac->error_code = legacy->error_code;
    hmac->reserved = 0;

    // Generate random salt
    if (!GenerateRandomSalt(hmac->salt, HMAC_SALT_SIZE)) return false;

    // Copy partial chain hash (first 8 bytes)
    if (chain_hash) {
        memcpy(hmac->prev_hash, chain_hash, 8);
    } else {
        memset(hmac->prev_hash, 0, 8);
    }

    // Compute HMAC signature
    if (!HMAC_ComputeSignature(hmac, hmac->signature)) return false;

    // Update chain hash for next entry
    if (chain_hash) {
        HMAC_UpdateChainHash(hmac, chain_hash);
    }

    return true;
}

void FlashLogger_ConvertFromHMAC(const LogEntryHMAC_t* hmac, LogEntry_t* legacy)
{
    if (!hmac || !legacy) return;

    legacy->timestamp = hmac->timestamp;
    legacy->duty_cycle = hmac->duty_cycle;
    legacy->efficiency = hmac->efficiency;
    legacy->temperature = hmac->temperature;
    legacy->current = hmac->current;
    legacy->error_code = hmac->error_code;
    legacy->crc = 0; // CRC not used in HMAC mode
}

bool FlashLoggerHMAC_Write(FlashLogger_t* logger, const LogEntry_t* entry)
{
    if (logger == NULL || entry == NULL || !logger->initialized) return false;

    // Convert to HMAC format
    LogEntryHMAC_t hmac_entry;
    FlashHeaderHMAC_t* header = (FlashHeaderHMAC_t*)FLASH_LOG_START_ADDR;
    uint8_t chain_hash[32];

    // Get current chain hash
    memcpy(chain_hash, header->chain_hash, 32);

    if (!FlashLogger_ConvertToHMAC(entry, &hmac_entry, chain_hash)) {
        return false;
    }

    // Calculate write address
    uint32_t addr = GetNextWriteAddress(logger);

    // Check if we need to wrap
    uint32_t max_entries = (FLASH_LOG_SIZE - sizeof(FlashHeaderHMAC_t)) / FLASH_HMAC_ENTRY_SIZE;
    if (logger->entry_count >= max_entries) {
        logger->wrap_count++;
        logger->entry_count = 0;
        addr = FLASH_LOG_START_ADDR + sizeof(FlashHeaderHMAC_t);

        // When wrapping, reset chain hash for the new cycle
        memset(chain_hash, 0, 32);
        if (!FlashLogger_ConvertToHMAC(entry, &hmac_entry, chain_hash)) {
            return false;
        }
    }

    // Write entry to flash
    if (!WriteLogEntry(addr, &hmac_entry)) return false;

    // Update header
    logger->entry_count++;
    header->write_index = addr + sizeof(LogEntryHMAC_t) - FLASH_LOG_START_ADDR;
    header->entry_count = logger->entry_count;
    header->wrap_count = logger->wrap_count;
    memcpy(header->chain_hash, chain_hash, 32);

    return true;
}

uint32_t FlashLoggerHMAC_Read(FlashLogger_t* logger, uint32_t index,
                              LogEntry_t* entry, bool* tampered)
{
    if (logger == NULL || entry == NULL || !logger->initialized) return 0;
    if (index >= logger->entry_count) return 0;

    // Calculate actual address with wear leveling
    uint32_t max_entries = (FLASH_LOG_SIZE - sizeof(FlashHeaderHMAC_t)) / FLASH_HMAC_ENTRY_SIZE;
    uint32_t actual_idx = (logger->entry_count - index - 1) % max_entries;
    uint32_t addr = FLASH_LOG_START_ADDR + sizeof(FlashHeaderHMAC_t) + (actual_idx * FLASH_HMAC_ENTRY_SIZE);

    // Read HMAC entry
    LogEntryHMAC_t hmac_entry;
    memcpy(&hmac_entry, (void*)addr, sizeof(LogEntryHMAC_t));

    // Verify HMAC signature
    bool valid = HMAC_VerifySignature(&hmac_entry);

    if (tampered) {
        *tampered = !valid;
    }

    // Convert to legacy format
    FlashLogger_ConvertFromHMAC(&hmac_entry, entry);

    return sizeof(LogEntry_t);
}

bool FlashLoggerHMAC_VerifyLog(const FlashLogger_t* logger,
                                uint32_t* valid_entries,
                                uint32_t* tampered_entries,
                                bool* chain_broken)
{
    if (logger == NULL || !logger->initialized) return false;

    uint32_t valid = 0;
    uint32_t tampered = 0;
    bool chain_ok = true;

    uint8_t expected_chain[32];
    memset(expected_chain, 0, 32);

    uint32_t max_entries = (FLASH_LOG_SIZE - sizeof(FlashHeaderHMAC_t)) / FLASH_HMAC_ENTRY_SIZE;
    uint32_t entries_to_check = logger->entry_count < max_entries ?
                                logger->entry_count : max_entries;

    for (uint32_t i = 0; i < entries_to_check; i++) {
        // Calculate address (oldest first)
        uint32_t actual_idx = (logger->entry_count - entries_to_check + i) % max_entries;
        uint32_t addr = FLASH_LOG_START_ADDR + sizeof(FlashHeaderHMAC_t) + (actual_idx * FLASH_HMAC_ENTRY_SIZE);

        LogEntryHMAC_t entry;
        memcpy(&entry, (void*)addr, sizeof(LogEntryHMAC_t));

        // Verify HMAC signature
        if (HMAC_VerifySignature(&entry)) {
            valid++;

            // Check chain integrity
            if (memcmp(entry.prev_hash, expected_chain, 8) != 0 && i > 0) {
                chain_ok = false;
            }

            // Update expected chain hash
            HMAC_UpdateChainHash(&entry, expected_chain);
        } else {
            tampered++;
            chain_ok = false;
        }
    }

    if (valid_entries) *valid_entries = valid;
    if (tampered_entries) *tampered_entries = tampered;
    if (chain_broken) *chain_broken = !chain_ok;

    return (tampered == 0) && chain_ok;
}

bool FlashLoggerHMAC_Clear(FlashLogger_t* logger)
{
    if (logger == NULL) return false;

    if (!EraseLogSector()) return false;

    // Reset state
    logger->write_index = sizeof(FlashHeaderHMAC_t);
    logger->entry_count = 0;
    logger->wrap_count = 0;

    // Generate new chain hash
    uint8_t new_chain[32];
    if (!GenerateRandomSalt(new_chain, 32)) {
        memset(new_chain, 0, 32);
    }

    // Write new HMAC header
    FlashHeaderHMAC_t header = { 0 };
    header.magic = FLASH_LOG_HMAC_MAGIC;
    header.version = 1;
    header.write_index = logger->write_index;
    header.entry_count = 0;
    header.wrap_count = 0;
    memcpy(header.chain_hash, new_chain, 32);

    return WriteFlashHeader(&header);
}

void FlashLoggerHMAC_GetStats(const FlashLogger_t* logger, char* buffer, uint16_t size)
{
    if (logger == NULL || buffer == NULL) return;

    uint32_t max_entries = (FLASH_LOG_SIZE - sizeof(FlashHeaderHMAC_t)) / FLASH_HMAC_ENTRY_SIZE;

    // Verify log integrity
    uint32_t valid, tampered;
    bool chain_broken;
    FlashLoggerHMAC_VerifyLog(logger, &valid, &tampered, &chain_broken);

    snprintf(buffer, size,
        "Flash Log (HMAC-SHA256):\r\n"
        "  Entries: %lu/%lu\r\n"
        "  Valid: %lu, Tampered: %lu\r\n"
        "  Chain: %s\r\n"
        "  Wraps: %lu\r\n"
        "  Wear level: %lu%%",
        (unsigned long)logger->entry_count,
        (unsigned long)max_entries,
        (unsigned long)valid,
        (unsigned long)tampered,
        chain_broken ? "BROKEN" : "OK",
        (unsigned long)logger->wrap_count,
        (unsigned long)((logger->entry_count % max_entries) * 100 / max_entries));
}
