/**
 * @file secure_bootloader.h
 * @brief Secure Bootloader API for AdaptivePWM
 * @version 1.0.0
 * @date 2026-04-18
 * 
 * @copyright AdaptivePWM Project
 * @license MIT
 */

#ifndef SECURE_BOOTLOADER_H
#define SECURE_BOOTLOADER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Firmware header magic number: "ADPW" in little-endian */
#define FIRMWARE_MAGIC              0x57445041UL

/** Bootloader size in bytes (16KB) */
#define BOOTLOADER_SIZE             (16 * 1024)

/** Application start address (after bootloader) */
#define APP_START_ADDR              0x08004000UL

/** Firmware header size in bytes */
#define FIRMWARE_HEADER_SIZE        128

/** Maximum firmware size (128KB - 16KB bootloader) */
#define MAX_FIRMWARE_SIZE           (112 * 1024)

/** Version storage address (within bootloader reserved area) */
#define VERSION_STORAGE_ADDR        0x08003800UL

/** Recovery mode UART baud rate */
#define RECOVERY_BAUD_RATE          115200

/* Ed25519 constants */
#define ED25519_PUBLIC_KEY_SIZE     32
#define ED25519_PRIVATE_KEY_SIZE    32
#define ED25519_SIGNATURE_SIZE      64
#define SHA256_DIGEST_SIZE          32

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * @brief Firmware header structure
 * 
 * This structure is placed at the beginning of the firmware binary.
 * It contains metadata and cryptographic verification data.
 */
typedef struct {
    uint32_t magic;                      /**< Magic number: 0x57445041 ("ADPW") */
    uint32_t version;                    /**< Monotonic version number */
    uint32_t firmware_size;              /**< Size of firmware in bytes */
    uint8_t  hash[SHA256_DIGEST_SIZE];   /**< SHA-256 hash of firmware */
    uint8_t  signature[ED25519_SIGNATURE_SIZE]; /**< Ed25519 signature */
    uint8_t  reserved[16];               /**< Reserved for future use */
} __attribute__((packed)) FirmwareHeader_t;

/**
 * @brief Secure boot result codes
 */
typedef enum {
    SECURE_BOOT_OK = 0,                  /**< Firmware verified successfully */
    SECURE_BOOT_INVALID_MAGIC,           /**< Invalid magic number */
    SECURE_BOOT_SIZE_EXCEEDED,           /**< Firmware size exceeds maximum */
    SECURE_BOOT_ROLLBACK_DETECTED,       /**< Version rollback detected */
    SECURE_BOOT_HASH_MISMATCH,            /**< Firmware hash mismatch */
    SECURE_BOOT_SIGNATURE_INVALID        /**< Ed25519 signature invalid */
} SecureBootResult_t;

/* ============================================================================
 * FLASH PROTECTION
 * ============================================================================ */

/**
 * @brief Read Protection Levels
 * 
 * RDP Level 0: No protection
 * RDP Level 1: Read protection active (debug allowed)
 * RDP Level 2: Full protection (debug disabled) - IRREVERSIBLE
 */
typedef enum {
    RDP_LEVEL_0 = 0xAA,  /**< No protection */
    RDP_LEVEL_1 = 0x55,  /**< Read protection */
    RDP_LEVEL_2 = 0xCC   /**< Full protection (IRREVERSIBLE) */
} RDPLevel_t;

/**
 * @brief Set Read Protection Level
 * 
 * @warning RDP Level 2 is IRREVERSIBLE and disables debug interface!
 * 
 * @param level RDP level to set (0, 1, or 2)
 * @return true if successful, false otherwise
 */
bool Flash_SetRDPLevel(RDPLevel_t level);

/**
 * @brief Get current Read Protection Level
 * 
 * @return Current RDP level
 */
RDPLevel_t Flash_GetRDPLevel(void);

/**
 * @brief Check if flash is write-protected
 * 
 * @return true if protected, false otherwise
 */
bool Flash_IsWriteProtected(void);

/* ============================================================================
 * SECURE BOOT FUNCTIONS
 * ============================================================================ */

/**
 * @brief Initialize secure boot subsystem
 */
void SecureBoot_Init(void);

/**
 * @brief Verify firmware signature and integrity
 * 
 * Performs the following checks:
 * 1. Magic number validation
 * 2. Firmware size check
 * 3. Anti-rollback version check
 * 4. SHA-256 hash verification
 * 5. Ed25519 signature verification
 * 
 * @return SecureBootResult_t Result code
 */
SecureBootResult_t SecureBoot_VerifyFirmware(void);

/**
 * @brief Jump to application firmware
 * 
 * @warning This function does not return on success
 */
void SecureBoot_JumpToApplication(void);

/**
 * @brief Get string description of result code
 * 
 * @param result Result code
 * @return Human-readable description
 */
const char* SecureBoot_GetResultString(SecureBootResult_t result);

/* ============================================================================
 * RECOVERY MODE
 * ============================================================================ */

/**
 * @brief Enter recovery mode for firmware update
 * 
 * Recovery mode allows authenticated firmware updates via UART.
 * Requires physical access (button/jumper) for security.
 */
void RecoveryMode_Enter(void);

/**
 * @brief Check if recovery mode is requested
 * 
 * @return true if recovery mode requested, false otherwise
 */
bool RecoveryMode_IsRequested(void);

/**
 * @brief Process a firmware update packet
 * 
 * @param data Packet data
 * @param length Packet length
 * @return true if valid, false otherwise
 */
bool RecoveryMode_ProcessPacket(const uint8_t *data, uint32_t length);

/* ============================================================================
 * VERSION MANAGEMENT
 * ============================================================================ */

/**
 * @brief Get stored firmware version
 * 
 * @return Stored version number
 */
uint32_t SecureBoot_GetStoredVersion(void);

/**
 * @brief Update stored firmware version
 * 
 * @param version New version number
 * @return true if successful, false otherwise
 */
bool SecureBoot_UpdateStoredVersion(uint32_t version);

/**
 * @brief Check version against rollback protection
 * 
 * @param version Version to check
 * @return true if allowed, false if rollback detected
 */
bool SecureBoot_CheckAntiRollback(uint32_t version);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * @brief Compute SHA-256 hash
 * 
 * @param data Input data
 * @param len Data length
 * @param hash Output hash (32 bytes)
 */
void SecureBoot_SHA256(const uint8_t *data, uint32_t len, uint8_t *hash);

/**
 * @brief Verify Ed25519 signature
 * 
 * @param signature 64-byte signature
 * @param message Message data
 * @param msglen Message length
 * @param public_key 32-byte public key
 * @return true if valid, false otherwise
 */
bool SecureBoot_VerifyEd25519(const uint8_t *signature, 
                               const uint8_t *message, 
                               uint32_t msglen, 
                               const uint8_t *public_key);

#ifdef __cplusplus
}
#endif

#endif /* SECURE_BOOTLOADER_H */
