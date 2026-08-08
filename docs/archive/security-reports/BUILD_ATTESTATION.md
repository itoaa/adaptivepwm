# AdaptivePWM Build Attestation

**SEC-133: AdaptivePWM Development Toolchain Verification**

This document describes the build attestation process for the AdaptivePWM project, ensuring the integrity and security of the development toolchain.

## Framework Compliance

- **CISSP D3/D8**: Security Architecture / Software Development Security
- **NIST PR.DS-06**: Data Integrity
- **ISO 27001 A.8.8**: Technical compliance

## Toolchain Components

The following development tools are verified as part of the build process:

| Component | Version | Path | Checksum |
|-----------|---------|------|----------|
| PlatformIO | 6.1.19 | `/home/linuxbrew/.linuxbrew/bin/platformio` | `20517772d7343ad1387410c1344a6adc9c18c7c3cc5dba9036ac0ffa052c46ba` |
| GCC ARM None EABI | 12.3.1 20230626 | `/home/ola/.platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-gcc` | `6b2d82f129c723fb1fe353fa308251aea7e7f1054b84d1dbb2065f34ed944f77` |
| OpenOCD | 0.12.0-01004-g9ea7f3d64-dirty | `/home/ola/.platformio/packages/tool-openocd/bin/openocd` | `924370bd1db2c88b5ad3669a8b8d93b4bcdd87c4b9574dbed0635d0b9c252eb5` |

## Verification Process

The toolchain integrity is verified using the `tools/verify-toolchain.sh` script. For detailed procedures, see [VERIFICATION_PROCEDURE.md](tools/VERIFICATION_PROCEDURE.md).

1. Calculates SHA-256 checksums for each toolchain component
2. Compares calculated checksums against known-good values
3. Generates a JSON attestation report with verification results
4. Fails the build if any component fails verification

### Running Verification Locally

See [VERIFICATION_PROCEDURE.md](tools/VERIFICATION_PROCEDURE.md) for detailed instructions on running verification locally and in different modes.

### Verification Options

For detailed information about verification options and procedures, see [VERIFICATION_PROCEDURE.md](tools/VERIFICATION_PROCEDURE.md).

- `--strict`: Enable strict mode (fail on any checksum mismatch)
- `--verbose`: Enable verbose output
- `--help`: Show help message

## Attestation Reports

Verification results are stored in `build-attestations/` as JSON files with timestamps:

```json
{
  "tool": "toolchain-verification",
  "version": "1.0",
  "timestamp": "2026-06-18T11:30:46Z",
  "components": {
    "platformio": {
      "path": "/home/linuxbrew/.linuxbrew/bin/platformio",
      "checksum": "20517772d7343ad1387410c1344a6adc9c18c7c3cc5dba9036ac0ffa052c46ba",
      "version": "PlatformIO Core, version 6.1.19"
    },
    "gcc-arm-none-eabi": {
      "path": "/home/ola/.platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-gcc",
      "checksum": "6b2d82f129c723fb1fe353fa308251aea7e7f1054b84d1dbb2065f34ed944f77",
      "version": "arm-none-eabi-gcc (xPack GNU Arm Embedded GCC x86_64) 12.3.1 20230626"
    },
    "openocd": {
      "path": "/home/ola/.platformio/packages/tool-openocd/bin/openocd",
      "checksum": "924370bd1db2c88b5ad3669a8b8d93b4bcdd87c4b9574dbed0635d0b9c252eb5",
      "version": "xPack Open On-Chip Debugger 0.12.0-01004-g9ea7f3d64-dirty (2023-01-30-15:03)"
    }
  },
  "verification_status": "PASSED",
  "strict_mode": "false"
}
```

## CI/CD Integration

The verification process is integrated into the GitHub Actions workflow:

1. **Pre-build verification**: Toolchain integrity is verified before each build
2. **Attestation checking**: Generated attestation files are validated for JSON syntax
3. **Freshness validation**: Attestation timestamps are checked to ensure currency

## Security Considerations

- Toolchain binaries are verified against SHA-256 checksums before each build
- Verification failures will halt the build process
- Attestation reports provide an audit trail of toolchain integrity
- Regular verification ensures protection against toolchain tampering

## Risk Mitigation

**Risk ID**: R-004 (Score: 15) - Compromised tools could inject backdoors

**Mitigation Strategy**:
1. Automated verification of all toolchain components
2. Cryptographic checksum validation
3. Immediate build failure on verification failure
4. Detailed attestation reporting for audit purposes

## Maintenance

To update toolchain checksums when tools are upgraded:

1. Run `./tools/verify-toolchain.sh` to generate new checksums
2. Update the verification script with new checksum values
3. Update [VERIFICATION_PROCEDURE.md](tools/VERIFICATION_PROCEDURE.md) with new checksums
4. Commit changes to version control
5. Verify the updated script works correctly

This process ensures that only trusted, verified toolchain components are used in the build process, maintaining the integrity of the AdaptivePWM firmware. For detailed maintenance procedures, see [VERIFICATION_PROCEDURE.md](tools/VERIFICATION_PROCEDURE.md).