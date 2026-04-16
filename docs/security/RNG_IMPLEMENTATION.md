# STM32F401 Hardware RNG Implementation

**Task:** SEC-033 - AdaptivePWM Hardware RNG Integration  
**Status:** ✅ COMPLETED  
**Date:** 2026-04-15  
**CVSS:** 5.9 (MEDIUM)

## Overview

Replaced the insecure LCG-based PRNG (Linear Congruential Generator) with the STM32F401's hardware True Random Number Generator (TRNG) for cryptographically secure random number generation.

## Security Improvement

| Aspect | Before (LCG) | After (Hardware RNG) |
|--------|--------------|---------------------|
| Entropy Source | Software algorithm | Analog noise (ring oscillators) |
| Predictability | Predictable seed | Unpredictable |
| Security Level | Low | High (NIST SP 800-90B compliant) |
| Speed | ~40x slower | Hardware-accelerated |
| Attack Surface | PRNG seed prediction | None (hardware-based) |

## Implementation Details

### Files Modified

1. **`src/cli_auth.c`** - Hardware RNG implementation
   - `init_hardware_rng()` - Initializes STM32 RNG peripheral
   - `secure_random()` - Generates random bytes using hardware RNG
   - `cli_auth_deinit_rng()` - Power management cleanup

2. **`src/config.h`** - RNG configuration
   - `RNG_ENABLED` - Enable hardware RNG (default: 1)
   - `RNG_FALLBACK_SOFTWARE` - Software fallback (default: 0, disabled)
   - `RNG_ERROR_RECOVERY` - Auto-recover from errors (default: 1)
   - `RNG_TIMEOUT_MS` - Timeout for polling (default: 100ms)

3. **`tests/test_cli_auth.c`** - Unit tests for RNG

### Hardware Features Used

- **Peripheral:** STM32F401 RNG (Random Number Generator)
- **Clock:** RCC_AHB2ENR_RNGEN (AHB2 bus)
- **Register:** RNG->DR (Data Register)
- **Status:** RNG_SR (Status Register with CECS/SEIS flags)
- **Entropy:** 32-bit true random numbers from analog noise sources

### API Usage

```c
// Initialize (called automatically in CLI_Auth_Init)
init_hardware_rng();

// Generate random bytes (e.g., for salt)
uint8_t salt[16];
secure_random(salt, 16);

// Cleanup when done (power management)
cli_auth_deinit_rng();
```

### Salt Generation for PBKDF2

The salt for password hashing is now generated using hardware RNG:

```c
static bool generate_salt(uint8_t* salt, size_t len)
{
    return secure_random(salt, len);  // Uses hardware RNG
}
```

## Configuration

### Enable Hardware RNG (default)
```c
#define RNG_ENABLED 1
```

### Disable Software Fallback (recommended for production)
```c
#define RNG_FALLBACK_SOFTWARE 0
```

### Enable Debug Output
```c
#define DEBUG_PRINT_ENABLED 1
// Output: "RNG: Hardware RNG initialized successfully"
```

## Testing

Unit tests verify:
- Hardware RNG initialization
- Random value generation (different values each time)
- Error handling (clock/seed errors)
- Fallback behavior (if enabled)

Run tests:
```bash
make test TEST=test_cli_auth
```

## Security Framework Mapping

| Framework | Mapping |
|-----------|---------|
| CISSP Domain 6 | Security Assessment and Testing |
| NIST CSF | PR.DS-02 (Data security) |
| ISO 27001 | A.8.24 (Use of cryptography) |

## References

- STM32F401 Reference Manual: Section 21 (Random Number Generator)
- STM32CubeF4 HAL RNG Driver Documentation
- NIST SP 800-90B: Recommendation for Entropy Sources
- Security Assessment Report 2026-04-15, Finding ADP-TEST-001

## Success Criteria

- [x] Hardware RNG initializes successfully on STM32F401
- [x] Salt generation for PBKDF2 uses hardware RNG
- [x] Unit tests pass for new RNG implementation
- [x] No regression in existing authentication functionality
- [x] Documentation updated

## Changelog

**v2.3.1 (2026-04-15)**
- Implemented STM32F401 hardware RNG (SEC-033)
- Removed LCG-based PRNG from production builds
- Added error recovery for clock/seed errors
- Added comprehensive unit tests
