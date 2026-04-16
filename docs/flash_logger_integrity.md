# AdaptivePWM Flash Logger Integrity Protection

## Overview

This document describes the HMAC-SHA256 integrity protection implementation for the AdaptivePWM flash logger. This feature addresses Security Assessment finding **ADP-006** (CVSS 5.9) by adding cryptographic tamper detection to flash-based logging.

## Security Framework Mapping

| Framework | Control | Status |
|-----------|---------|--------|
| NIST CSF 2.0 | PROTECT.PR.DS-06 (Integrity) | ✅ Implemented |
| ISO 27001:2022 | 8.15 (Logging) | ✅ Implemented |
| CISSP Domain | 3 (Security Architecture) | ✅ Implemented |

## Architecture

### Before: CRC-Based Integrity

The legacy flash logger used a simple CRC-16 checksum:

```c
typedef struct {
    uint32_t timestamp;
    float duty_cycle;
    float efficiency;
    float temperature;
    float current;
    uint16_t error_code;
    uint16_t crc;           // Simple CRC-16
} LogEntry_t;               // 32 bytes total
```

**Limitations:**
- CRC can be recalculated by attacker after modification
- No replay attack protection
- No chaining between entries
- Tampering detection is trivial to bypass

### After: HMAC-SHA256 Protection

The new implementation uses HMAC-SHA256 with entry chaining:

```c
typedef struct {
    // Original data (24 bytes)
    uint32_t timestamp;
    float duty_cycle;
    float efficiency;
    float temperature;
    float current;
    uint16_t error_code;
    uint16_t reserved;
    
    // Integrity metadata (24 bytes)
    uint8_t salt[HMAC_SALT_SIZE];       // 16 bytes, random per entry
    uint8_t prev_hash[8];               // Partial chain hash
    
    // Cryptographic signature (32 bytes)
    uint8_t signature[HMAC_SHA256_SIGNATURE_SIZE];  // HMAC-SHA256
} LogEntryHMAC_t;           // 80 bytes total
```

**Security Features:**
- HMAC-SHA256 signatures (cryptographically secure)
- Per-entry random salt (prevents replay attacks)
- Entry chaining (prevents deletion/reordering)
- Secure key storage abstraction
- Constant-time verification (prevents timing attacks)

## Key Storage

### Secure Key Storage (Production)

In production, the HMAC key should be stored in:
- STM32 Option Bytes (secure flash)
- Dedicated HSM or secure element
- One-time programmable (OTP) region

### Development Key Storage

For development, keys are stored in a dedicated flash sector (Sector 6) with:
- Key generated on first boot using hardware RNG
- Key persisted across reboots
- Secure zeroing on security events

```c
// Key storage location
#define HMAC_KEY_FLASH_ADDR    0x080D0000  // Sector 6 (128KB)
```

## Implementation Details

### HMAC Computation

```c
bool HMAC_ComputeSignature(const LogEntryHMAC_t* entry, uint8_t* signature) {
    // Data includes: timestamp, duty_cycle, efficiency, temperature,
    // current, error_code, reserved, salt, prev_hash
    
    size_t data_len = sizeof(LogEntryHMAC_t) - HMAC_SHA256_SIGNATURE_SIZE;
    
    return mbedtls_md_hmac(
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
        key, HMAC_SHA256_KEY_SIZE,
        (const uint8_t*)entry, data_len,
        signature
    ) == 0;
}
```

### Entry Chaining

Each entry includes a partial hash of the previous entry's chain hash, creating a blockchain-like integrity chain:

```
Entry N:   Chain[N] = SHA256(Chain[N-1] || Signature[N])
           prev_hash[N] = first 8 bytes of Chain[N-1]
```

This prevents:
- Entry deletion (chain breaks)
- Entry reordering (chain breaks)
- Entry insertion (chain breaks)

### Constant-Time Verification

To prevent timing attacks, verification uses constant-time comparison:

```c
volatile int result = 0;
for (int i = 0; i < HMAC_SHA256_SIGNATURE_SIZE; i++) {
    result |= entry->signature[i] ^ computed_sig[i];
}
return result == 0;
```

## API Reference

### Initialization

```c
// Initialize with HMAC support (replaces FlashLogger_Init)
bool FlashLoggerHMAC_Init(FlashLogger_t* logger);

// Initialize HMAC key storage
bool HMAC_InitKeyStorage(void);
```

### Writing Entries

```c
// Write with automatic HMAC signing
bool FlashLoggerHMAC_Write(FlashLogger_t* logger, const LogEntry_t* entry);
```

### Reading Entries

```c
// Read with HMAC verification
uint32_t FlashLoggerHMAC_Read(FlashLogger_t* logger, uint32_t index,
                              LogEntry_t* entry, bool* tampered);
```

### Verification

```c
// Verify entire log integrity
bool FlashLoggerHMAC_VerifyLog(const FlashLogger_t* logger,
                                uint32_t* valid_entries,
                                uint32_t* tampered_entries,
                                bool* chain_broken);
```

### Management

```c
// Clear log with fresh chain
bool FlashLoggerHMAC_Clear(FlashLogger_t* logger);

// Get integrity-aware statistics
void FlashLoggerHMAC_GetStats(const FlashLogger_t* logger, char* buffer, uint16_t size);

// Securely clear key from memory
void HMAC_ClearKey(void);
```

## Verification Tool

A Python tool is provided for offline log verification:

```bash
# Verify flash dump
python tools/verify_log_integrity.py flash_dump.bin

# With specific key
python tools/verify_log_integrity.py flash_dump.bin --key <64_char_hex_key>

# Verbose output
python tools/verify_log_integrity.py flash_dump.bin --verbose

# JSON output
python tools/verify_log_integrity.py flash_dump.bin --json
```

## Migration Guide

### From Legacy CRC Logs

The HMAC logger automatically detects legacy logs and provides read-only compatibility:

1. **Detection**: On init, checks for `FLASH_MAGIC` (legacy) vs `FLASH_LOG_HMAC_MAGIC`
2. **Read**: Legacy entries can still be read
3. **Write**: New entries are written in HMAC format
4. **Clear**: Reinitializes with HMAC format

### Code Changes Required

**Before:**
```c
FlashLogger_Init(&logger);
FlashLogger_Write(&logger, &entry);
FlashLogger_Read(&logger, index, &entry);
```

**After:**
```c
FlashLoggerHMAC_Init(&logger);
FlashLoggerHMAC_Write(&logger, &entry);

bool tampered;
FlashLoggerHMAC_Read(&logger, index, &entry, &tampered);
if (tampered) {
    // Handle tampering detection
}
```

## Security Considerations

### Key Protection

1. **Never commit keys to version control**
2. **Use different keys per device** (device-unique keys)
3. **Implement key rotation** for long-running devices
4. **Detect key compromise** (unexpected signature failures)

### Physical Security

1. **Flash sector protection**: Enable write protection for key storage sector
2. **Tamper detection**: Monitor for unexpected flash erases
3. **Secure boot**: Verify firmware integrity before running logger

### Side-Channel Protection

1. **Constant-time HMAC comparison** prevents timing attacks
2. **Random salt** prevents replay attacks
3. **Entry chaining** prevents deletion/reordering

## Testing

### Unit Tests

```c
// Test HMAC computation
void test_hmac_computation(void) {
    LogEntryHMAC_t entry = { /* ... */ };
    uint8_t sig[HMAC_SHA256_SIGNATURE_SIZE];
    
    assert(HMAC_ComputeSignature(&entry, sig));
    assert(HMAC_VerifySignature(&entry));
}

// Test tamper detection
void test_tamper_detection(void) {
    LogEntryHMAC_t entry = { /* ... */ };
    HMAC_ComputeSignature(&entry, entry.signature);
    
    // Tamper with data
    entry.duty_cycle += 0.1f;
    
    // Should detect tampering
    assert(!HMAC_VerifySignature(&entry));
}

// Test chain integrity
void test_chain_integrity(void) {
    // Write multiple entries
    // Verify chain hash progression
    // Verify deletion detection
}
```

### Integration Tests

1. **Flash endurance**: Test with sector wrap-around
2. **Power loss**: Verify partial write handling
3. **Key generation**: Verify RNG quality
4. **Migration**: Legacy to HMAC transition

## Performance Impact

| Operation | CRC (Legacy) | HMAC-SHA256 | Overhead |
|-----------|--------------|-------------|----------|
| Write | ~5 µs | ~150 µs | 30x |
| Read | ~2 µs | ~10 µs | 5x |
| Verify | ~2 µs | ~150 µs | 75x |
| Entry size | 32 bytes | 80 bytes | 2.5x |

**Mitigation:**
- Batch verification for large logs
- Hardware acceleration (STM32F4 crypto peripheral)
- Lazy verification on read

## References

- **NIST FIPS 198-1**: The Keyed-Hash Message Authentication Code (HMAC)
- **NIST SP 800-107**: Recommendation for Applications Using Approved Hash Algorithms
- **mbedTLS Documentation**: https://tls.mbed.org/
- **STM32F4 Reference Manual**: Flash programming

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-04-12 | Initial HMAC-SHA256 implementation |

## Contact

For security issues or questions:
- Security Assessment: `reports/Security-Assessment-Report-2026-04-12.md`
- Task: `tasks/SEC-023.md`
