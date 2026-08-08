/**
 * @file flash_logger_hmac.h
 * @brief HMAC-SHA256 integrity protection for flash logger entries
 * 
 * Implements NIST CSF PROTECT.PR.DS-06 (Data Integrity) and
 * ISO 27001 8.15 (Logging) requirements.
 * 
 * Security features:
 * - HMAC-SHA256 signatures for tamper detection
 * - Secure key storage abstraction
 * - Entry chaining for sequence integrity
 * - Verification API for integrity checks
 * 
 * @version 1.0.0
 * @date 2026-04-12
 */

#ifndef FLASH_LOGGER_HMAC_H
#define FLASH_LOGGER_HMAC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "flash_logger.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// HMAC-SHA256 CONFIGURATION
// =============================================================================

#define HMAC_SHA256_KEY_SIZE        32      // 256-bit key
#define HMAC_SHA256_SIGNATURE_SIZE  32      // 256-bit signature
#define HMAC_SALT_SIZE              16      // 128-bit salt per entry

// Magic values for header validation
#define FLASH_LOG_HMAC_MAGIC        0x484D4143  // "HMAC"
#define FLASH_LOG_CHAIN_MAGIC       0x43484131  // "CHA1" (Chain v1)

// =============================================================================
// LOG ENTRY FORMAT WITH HMAC (48 bytes total)
// =============================================================================

/**
 * @brief Log entry with HMAC-SHA256 integrity protection
 * 
 * Extended log entry format that replaces the CRC-based entry
 * with cryptographically secure HMAC-SHA256 signatures.
 * 
 * Entry format:
 * - timestamp:      4 bytes (uint32_t)
 * - duty_cycle:     4 bytes (float)
 * - efficiency:     4 bytes (float)
 * - temperature:    4 bytes (float)
 * - current:        4 bytes (float)
 * - error_code:     2 bytes (uint16_t)
 * - reserved:       2 bytes (padding)
 * - salt:          16 bytes (random per-entry)
 * - prev_hash:     8 bytes (partial previous entry hash)
 * - signature:     32 bytes (HMAC-SHA256)
 * 
 * Total: 80 bytes (aligned to 4 bytes)
 */
typedef struct {
    // Original log data (24 bytes)
    uint32_t timestamp;          // Unix timestamp
    float duty_cycle;          // PWM duty cycle
    float efficiency;          // System efficiency
    float temperature;         // Temperature reading
    float current;             // Current measurement
    uint16_t error_code;       // Error code
    uint16_t reserved;         // Padding to align
    
    // Integrity metadata (24 bytes)
    uint8_t salt[HMAC_SALT_SIZE];           // Random salt (prevents replay)
    uint8_t prev_hash[8];                   // Partial chain hash
    
    // Cryptographic signature (32 bytes)
    uint8_t signature[HMAC_SHA256_SIGNATURE_SIZE]; // HMAC-SHA256
} LogEntryHMAC_t;

// Verify size is exactly 80 bytes
_Static_assert(sizeof(LogEntryHMAC_t) == 80, "LogEntryHMAC_t must be 80 bytes");

// =============================================================================
// FLASH HEADER WITH HMAC SUPPORT
// =============================================================================

/**
 * @brief Flash log header with HMAC chain information
 */
typedef struct {
    uint32_t magic;              // FLASH_LOG_HMAC_MAGIC
    uint32_t version;            // Format version (1)
    uint32_t write_index;        // Current write position
    uint32_t entry_count;        // Total entries written
    uint32_t wrap_count;         // Number of wraps
    uint8_t  chain_hash[32];     // Running chain hash (SHA256)
    uint8_t  reserved[12];       // Padding
} FlashHeaderHMAC_t;

// =============================================================================
// KEY MANAGEMENT
// =============================================================================

/**
 * @brief Key storage interface
 * 
 * Abstraction layer for secure key storage.
 * Actual implementation depends on hardware capabilities:
 * - STM32: Can use Option Bytes or separate flash sector
 * - Secure element: Use dedicated HSM/AES peripheral
 * - Development: Software key in RAM (NOT FOR PRODUCTION)
 */
typedef struct {
    uint8_t key[HMAC_SHA256_KEY_SIZE];  // HMAC key (256-bit)
    bool initialized;                      // Key loaded flag
} HMAC_KeyStorage_t;

/**
 * @brief Initialize HMAC key storage
 * 
 * Loads or derives the HMAC signing key from secure storage.
 * Must be called before any logging operations.
 * 
 * @return true on success, false on failure
 */
bool HMAC_InitKeyStorage(void);

/**
 * @brief Get the current HMAC key
 * 
 * Returns pointer to key storage (for signing operations).
 * Key should never be exposed outside this module.
 * 
 * @return Pointer to key storage, or NULL if not initialized
 */
const HMAC_KeyStorage_t* HMAC_GetKeyStorage(void);

/**
 * @brief Securely clear key from memory
 * 
 * Wipes the key from RAM using secure zeroing.
 * Called on shutdown or security event.
 */
void HMAC_ClearKey(void);

// =============================================================================
// HMAC-SHA256 OPERATIONS
// =============================================================================

/**
 * @brief Compute HMAC-SHA256 signature
 * 
 * Calculates HMAC-SHA256 over the entry data using the stored key.
 * 
 * @param entry Entry to sign (salt must be populated)
 * @param[out] signature Output buffer (32 bytes)
 * @return true on success
 */
bool HMAC_ComputeSignature(const LogEntryHMAC_t* entry, uint8_t* signature);

/**
 * @brief Verify HMAC-SHA256 signature
 * 
 * Verifies that the entry's signature matches computed value.
 * 
 * @param entry Entry to verify
 * @return true if signature valid, false if tampered
 */
bool HMAC_VerifySignature(const LogEntryHMAC_t* entry);

/**
 * @brief Update chain hash for entry chaining
 * 
 * Computes running hash that links entries together.
 * Prevents deletion or reordering attacks.
 * 
 * @param entry Current entry
 * @param[in,out] chain_hash Previous chain hash, updated with new value
 */
void HMAC_UpdateChainHash(const LogEntryHMAC_t* entry, uint8_t* chain_hash);

// =============================================================================
// FLASH LOGGER HMAC API
// =============================================================================

/**
 * @brief Initialize flash logger with HMAC support
 * 
 * Extended initialization that checks for HMAC-formatted logs.
 * Falls back to legacy CRC format if HMAC not present.
 * 
 * @param[out] logger Logger state
 * @return true on success
 */
bool FlashLoggerHMAC_Init(FlashLogger_t* logger);

/**
 * @brief Write entry with HMAC signature
 * 
 * Signs entry with HMAC-SHA256 before writing to flash.
 * Automatically generates salt and updates chain hash.
 * 
 * @param logger Logger state
 * @param entry Legacy entry data (will be wrapped in HMAC format)
 * @return true on success
 */
bool FlashLoggerHMAC_Write(FlashLogger_t* logger, const LogEntry_t* entry);

/**
 * @brief Read entry with HMAC verification
 * 
 * Reads entry from flash and verifies HMAC signature.
 * Detects tampering or corruption.
 * 
 * @param logger Logger state
 * @param index Entry index (0 = oldest)
 * @param[out] entry Legacy entry structure to fill
 * @param[out] tampered Optional: set to true if HMAC verification failed
 * @return Number of bytes read, 0 on error
 */
uint32_t FlashLoggerHMAC_Read(FlashLogger_t* logger, uint32_t index, 
                               LogEntry_t* entry, bool* tampered);

/**
 * @brief Verify entire log integrity
 * 
 * Scans all entries and verifies HMAC signatures.
 * Reports tampered entries and chain integrity.
 * 
 * @param logger Logger state
 * @param[out] valid_entries Count of valid entries
 * @param[out] tampered_entries Count of tampered entries
 * @param[out] chain_broken true if chain sequence broken
 * @return true if all entries valid and chain intact
 */
bool FlashLoggerHMAC_VerifyLog(const FlashLogger_t* logger,
                                uint32_t* valid_entries,
                                uint32_t* tampered_entries,
                                bool* chain_broken);

/**
 * @brief Clear log with HMAC header reset
 * 
 * Erases flash sector and reinitializes with fresh HMAC header.
 * Generates new chain hash.
 * 
 * @param logger Logger state
 * @return true on success
 */
bool FlashLoggerHMAC_Clear(FlashLogger_t* logger);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Convert legacy entry to HMAC format
 * 
 * Wraps legacy LogEntry_t in HMAC-protected format.
 * Generates salt and computes signature.
 * 
 * @param legacy Legacy entry
 * @param[out] hmac HMAC entry to populate
 * @param chain_hash Current chain hash (updated)
 * @return true on success
 */
bool FlashLogger_ConvertToHMAC(const LogEntry_t* legacy,
                                LogEntryHMAC_t* hmac,
                                uint8_t* chain_hash);

/**
 * @brief Convert HMAC entry to legacy format
 * 
 * Extracts legacy data from HMAC-protected entry.
 * Does NOT verify signature - use HMAC_VerifySignature first.
 * 
 * @param hmac HMAC entry
 * @param[out] legacy Legacy entry to populate
 */
void FlashLogger_ConvertFromHMAC(const LogEntryHMAC_t* hmac, LogEntry_t* legacy);

/**
 * @brief Get HMAC log statistics
 * 
 * Returns statistics including integrity status.
 * 
 * @param logger Logger state
 * @param[out] buffer Output buffer
 * @param size Buffer size
 */
void FlashLoggerHMAC_GetStats(const FlashLogger_t* logger, char* buffer, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif // FLASH_LOGGER_HMAC_H
