# Secure Boot Implementation for AdaptivePWM

**Document ID:** SEC-049  
**Version:** 1.0.0  
**Date:** 2026-04-19  
**Security Framework:** NIST CSF PR.DS-06, ISO 27001 A.8.24, CISSP D3  
**Status:** IMPLEMENTED  

---

## Overview

This document describes the complete Secure Boot implementation for AdaptivePWM, addressing critical security finding **ADP-ARCH-001** (CVSS 8.1). The implementation provides cryptographic verification of firmware integrity using Ed25519 signatures before execution.

## Security Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│  Bootloader (16KB) - Read-protected (RDP Level 2)               │
│  ├── Ed25519 Signature Verification                            │
│  ├── SHA-256 Hash Verification                                 │
│  ├── Anti-rollback Version Check                                 │
│  └── Recovery Mode (authenticated update)                      │
└─────────────────────────────────────────────────────────────────┘
                           ↓ (If valid)
┌─────────────────────────────────────────────────────────────────┐
│  Application Firmware (Signed)                                 │
│  ├── Firmware Header (128 bytes)                                │
│  │   ├── Magic: 0x57445041 ("ADPW")                            │
│  │   ├── Version (monotonic)                                    │
│  │   ├── Firmware Size                                          │
│  │   ├── SHA-256 Hash                                           │
│  │   └── Ed25519 Signature (64 bytes)                           │
│  └── Application Code                                          │
└─────────────────────────────────────────────────────────────────┘
```

---

## Components

### 1. Bootloader (`bootloader/`)

The secure bootloader is a minimal 16KB component that:

- **Verifies firmware signatures** before booting
- **Implements anti-rollback protection** via monotonic version counter
- **Provides recovery mode** for authenticated firmware updates
- **Runs from protected flash** (RDP Level 2 in production)

#### Source Files

| File | Description |
|------|-------------|
| `secure_bootloader.c` | Main bootloader implementation with Ed25519/SHA-256 |
| `secure_bootloader.h` | Public API and data structures |
| `Makefile` | Build configuration for bootloader |
| `ldscripts/secure_bootloader.ld` | Linker script (16KB limit) |

#### Build

```bash
cd bootloader
make                    # Build bootloader
make size              # Show size report
make flash             # Flash to device (requires RDP Level 0 or 1)
make set-rdp1          # Enable read protection
make set-rdp2          # Enable full protection (PRODUCTION)
```

**⚠️ WARNING: RDP Level 2 is IRREVERSIBLE and disables debug interface!**

---

### 2. Firmware Signing Tool (`tools/sign_firmware.py`)

Signs firmware binaries with Ed25519 signatures for secure boot verification.

#### Prerequisites

```bash
pip install pynacl cryptography
```

#### Usage

```bash
# Generate new Ed25519 key pair
python3 tools/sign_firmware.py --generate-keys --key-dir ./keys

# Sign firmware
python3 tools/sign_firmware.py \
    firmware.bin \
    firmware_signed.bin \
    keys/bootloader_private.pem \
    --version 2

# Verify signature
python3 tools/sign_firmware.py \
    --verify \
    firmware_signed.bin \
    - \
    keys/bootloader_public.pem
```

#### Key Generation

```bash
# Generate Ed25519 key pair
python3 tools/sign_firmware.py --generate-keys --key-dir ./keys

# Output files:
#   keys/bootloader_private.pem    # 32-byte seed (keep secure!)
#   keys/bootloader_public.pem     # 32-byte public key
#   keys/bootloader_public_key.c   # C header for embedding
```

**🔐 Security Requirements:**
- Store private keys in HSM or secure offline storage
- Never commit private keys to version control
- Use separate keys for development and production
- Implement secure key generation ceremony

---

### 3. Firmware Header Structure

```c
typedef struct {
    uint32_t magic;              // 0x57445041 ("ADPW")
    uint32_t version;            // Monotonic version (anti-rollback)
    uint32_t firmware_size;      // Size in bytes
    uint8_t  hash[32];           // SHA-256 of firmware
    uint8_t  signature[64];      // Ed25519 signature
    uint8_t  reserved[16];       // Future use
} __attribute__((packed)) FirmwareHeader_t;
```

**Total Header Size:** 128 bytes

---

## Verification Flow

```
Power On
    ↓
Bootloader Init
    ↓
Check Recovery Mode? ──Yes──→ Recovery Mode (UART update)
    │ No
    ↓
Read Firmware Header
    ↓
Validate Magic Number
    ↓
Check Firmware Size (< 112KB)
    ↓
Anti-rollback Check (version >= stored)
    ↓
Calculate SHA-256 Hash
    ↓
Verify Ed25519 Signature
    ↓
All Valid? ──Yes──→ Jump to Application
    │ No
    ↓
Recovery Mode
```

---

## Flash Protection (RDP)

### Read Protection Levels

| Level | Description | Debug Interface |
|-------|-------------|-----------------|
| **0** | No protection | Enabled |
| **1** | Read protection | Enabled, flash read disabled |
| **2** | Full protection | **Disabled permanently** |

### Setting RDP Level 2 (Production)

**⚠️ WARNING: IRREVERSIBLE - Debug interface will be disabled!**

```bash
cd scripts
./enable_protection.sh --yes-i-am-sure
```

Or using the production flash script:

```bash
# Production flash with RDP Level 2
cd scripts
./enable_protection.sh --dry-run    # Preview changes
./enable_protection.sh              # Enable with confirmation
```

**Consequences of RDP Level 2:**
- ✓ Flash cannot be read externally
- ✓ Firmware cannot be dumped
- ✓ Bootloader is protected
- ✗ Debug interface disabled permanently
- ✗ Mass erase disabled
- ✗ Recovery requires physical intervention

### Production Flash Script (`scripts/enable_protection.sh`)

Automates bootloader flashing and RDP Level 2 enable:

```bash
# Usage
./enable_protection.sh [--dry-run] [--yes-i-am-sure]

# Preview changes
./enable_protection.sh --dry-run

# Enable with confirmation prompts
./enable_protection.sh

# Skip confirmation (USE WITH CAUTION!)
./enable_protection.sh --yes-i-am-sure
```

---

## Recovery Mode

Recovery mode allows firmware updates when:
- Signature verification fails
- Anti-rollback blocks update
- Recovery mode button is pressed

### Entering Recovery Mode

1. **Automatic entry:** Signature verification failure
2. **Manual entry:** Hold recovery button (PA0) during power-on

### Recovery Mode Protocol

```
[HOST]  → "ADPW\n"                          [DEVICE]
[HOST]  ← "READY\n"                         [DEVICE]
[HOST]  → <header:128 bytes>               [DEVICE]
[HOST]  ← "HEADER_OK\n" or "HEADER_FAIL\n" [DEVICE]
[HOST]  → <firmware:data>                  [DEVICE]
[HOST]  ← "SIGNATURE_OK\n" or "SIG_FAIL\n"  [DEVICE]
[HOST]  → "CONFIRM\n"                       [DEVICE]
[HOST]  ← "WRITING\n"                       [DEVICE]
[HOST]  ← "DONE\n"                          [DEVICE]
```

---

## Build Integration

### Makefile Integration

Add to main `Makefile`:

```makefile
# Firmware signing
FIRMWARE_SIGN_KEY ?= keys/firmware_private.pem
FIRMWARE_VERSION ?= 1

signed-firmware: firmware.bin
	python3 tools/sign_firmware.py \
		$< \
		firmware_signed.bin \
		$(FIRMWARE_SIGN_KEY) \
		--version $(FIRMWARE_VERSION)

# Combined bootloader + signed firmware
production-image: bootloader
	python3 tools/combine_images.py \
		bootloader/build/secure_bootloader.bin \
		firmware_signed.bin \
		production_image.bin
```

### CI/CD Integration

```yaml
# .github/workflows/build.yml
- name: Sign Firmware
  run: |
    python3 tools/sign_firmware.py \
      build/firmware.bin \
      build/firmware_signed.bin \
      <(echo "$FIRMWARE_PRIVATE_KEY" | base64 -d) \
      --version ${{ github.run_number }}
  env:
    FIRMWARE_PRIVATE_KEY: ${{ secrets.FIRMWARE_PRIVATE_KEY }}
```

---

## Testing

### Unit Tests

```bash
# Run security tests
cd tests/security
make test-secure-boot

# Test vectors
test_secure_boot \
    --test-vector vectors/valid_signature.bin \
    --test-vector vectors/invalid_signature.bin \
    --test-vector vectors/wrong_version.bin
```

### Integration Tests

| Test Case | Expected Result |
|-----------|-----------------|
| Valid firmware | Boot success |
| Invalid signature | Enter recovery mode |
| Corrupted hash | Signature verification fails |
| Rollback attempt | Anti-rollback blocks |
| Recovery mode | Accepts valid update |

---

## Security Considerations

### Key Management

1. **Private Key Storage**
   - Use HSM (YubiHSM, AWS KMS) for production
   - Store offline in secure location
   - Implement key rotation procedure

2. **Public Key Distribution**
   - Embedded in bootloader binary
   - C header auto-generated by signing tool
   - Verify hash at build time

3. **Key Generation Ceremony**
   - Generate keys offline
   - Multiple witnesses
   - Document in secure log

### Attack Vectors Addressed

| Attack | Mitigation |
|--------|------------|
| Firmware replacement | Ed25519 signature verification |
| Rollback to vulnerable version | Monotonic version counter |
| Debug interface exploitation | RDP Level 2 |
| Flash dump | RDP Level 2 + bootloader protection |
| Supply chain attack | Signed firmware only |
| Key compromise | HSM storage + key rotation |

---

## Deployment Checklist

Before enabling RDP Level 2 in production:

- [ ] Bootloader tested and working
- [ ] Ed25519 verification validates correctly
- [ ] Recovery mode functional
- [ ] Production firmware signed and tested
- [ ] Private keys stored securely
- [ ] Recovery procedure documented
- [ ] Device serial numbers documented
- [ ] Production team trained on recovery process

---

## References

- **Security Assessment:** `reports/Security-Assessment-Report-2026-04-19.md` (ADP-ARCH-001)
- **Previous Documentation:** `projects/AdaptivePWM/docs/secure-boot.md` (SEC-045)
- **Threat Model:** `projects/AdaptivePWM/docs/security/threat-model.md`
- **Ed25519:** RFC 8032
- **STM32 Read Protection:** STM32F4 Reference Manual (Section on Read Protection)
- **NIST CSF:** PR.DS-06 (Integrity verification)
- **ISO 27001:** A.8.24 (Secure development lifecycle)

---

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-04-19 | Complete implementation (SEC-049) |
| 0.9.0 | 2026-04-18 | Documentation (SEC-045) |

---

## Appendix

### A. Ed25519 Implementation

The bootloader uses a minimal Ed25519 implementation based on TweetNaCl (public domain). For production use, consider:

- Hardware acceleration (STM32 crypto library)
- Formal verification of implementation
- Side-channel resistance measures

### B. Memory Map

```
0x08000000 - 0x08003FFF : Bootloader (16KB)
0x08004000 - 0x0801FFFF : Application Firmware (112KB)
0x20000000 - 0x20017FFF : SRAM (96KB)
```

### C. Recovery Mode Protocol Detail

```c
/* Firmware update protocol */
typedef enum {
    RECOVERY_STATE_IDLE = 0,
    RECOVERY_STATE_HEADER,
    RECOVERY_STATE_DATA,
    RECOVERY_STATE_VERIFY,
    RECOVERY_STATE_WRITE,
    RECOVERY_STATE_DONE
} RecoveryState_t;

/* Packet types */
#define PACKET_HEADER   0x01
#define PACKET_DATA     0x02
#define PACKET_VERIFY   0x03
#define PACKET_COMMIT   0x04
```

### D. RDP Level Detection

```c
RDPLevel_t GetRDPLevel(void) {
    uint16_t rdp = (FLASH->OPTCR >> 8) & 0xFF;
    
    if (rdp == 0xAA) return RDP_LEVEL_0;
    if (rdp == 0xCC) return RDP_LEVEL_2;
    return RDP_LEVEL_1;
}
```

### E. Secure Boot Result Codes

```c
typedef enum {
    SECURE_BOOT_OK = 0,
    SECURE_BOOT_INVALID_MAGIC,
    SECURE_BOOT_SIZE_EXCEEDED,
    SECURE_BOOT_ROLLBACK_DETECTED,
    SECURE_BOOT_HASH_MISMATCH,
    SECURE_BOOT_SIGNATURE_INVALID
} SecureBootResult_t;
```
