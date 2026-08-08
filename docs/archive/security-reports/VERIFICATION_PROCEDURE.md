# Toolchain Verification Procedure

**SEC-133: AdaptivePWM Development Toolchain Verification**

This document provides a detailed step-by-step procedure for verifying the integrity of the development toolchain used in the AdaptivePWM project.

## Framework Compliance

- **CISSP D3/D8**: Security Architecture / Software Development Security
- **NIST PR.DS-06**: Data Integrity
- **ISO 27001 A.8.8**: Technical compliance

## Prerequisites

Before running the verification procedure, ensure:

1. PlatformIO Core 6.1.19 is installed
2. GCC ARM None EABI 12.3.1 20230626 is installed
3. OpenOCD 0.12.0-01004-g9ea7f3d64-dirty is installed
4. The `verify-toolchain.sh` script is executable
5. The `build-attestations` directory can be created/modified

## Verification Procedure

### 1. Local Verification

To verify the toolchain integrity locally:

```bash
cd projects/AdaptivePWM
./tools/verify-toolchain.sh --verbose
```

This command will:
- Calculate SHA-256 checksums for each toolchain component
- Compare calculated checksums against known-good values
- Display version information for each component
- Generate a JSON attestation report
- Exit with status 0 on success, 1 on failure

### 2. Strict Mode Verification

For strict verification that fails on any checksum mismatch:

```bash
cd projects/AdaptivePWM
./tools/verify-toolchain.sh --strict --verbose
```

### 3. Verification Options

The verification script supports the following options:

- `--strict`: Enable strict mode (fail on any checksum mismatch)
- `--verbose`: Enable verbose output
- `--help`: Show help message

### 4. Verification Process Details

The verification process performs the following steps:

1. **Checksum Calculation**: Calculates SHA-256 checksums for each toolchain binary
2. **Checksum Comparison**: Compares calculated checksums against known-good values
3. **Version Detection**: Retrieves version information for each tool
4. **Attestation Generation**: Creates a JSON report with verification results
5. **Status Reporting**: Exits with appropriate status code

### 5. Toolchain Components Verified

The following components are verified:

| Component | Path | Expected Checksum |
|-----------|------|-------------------|
| PlatformIO | `/home/linuxbrew/.linuxbrew/bin/platformio` | `20517772d7343ad1387410c1344a6adc9c18c7c3cc5dba9036ac0ffa052c46ba` |
| GCC ARM None EABI | `/home/ola/.platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-gcc` | `6b2d82f129c723fb1fe353fa308251aea7e7f1054b84d1dbb2065f34ed944f77` |
| OpenOCD | `/home/ola/.platformio/packages/tool-openocd/bin/openocd` | `924370bd1db2c88b5ad3669a8b8d93b4bcdd87c4b9574dbed0635d0b9c252eb5` |

### 6. Attestation Reports

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

## Maintenance Procedure

To update toolchain checksums when tools are upgraded:

1. Install the new version of the tool
2. Run `./tools/verify-toolchain.sh` to see the new checksum
3. Update the verification script with the new checksum values
4. Update this documentation with the new checksums
5. Commit changes to version control
6. Verify the updated script works correctly

## Troubleshooting

### Common Issues

1. **Toolchain binary not found**: Ensure the tool is installed in the expected location
2. **Checksum mismatch**: Verify the tool version matches expected versions
3. **Permission denied**: Ensure the script has execute permissions

### Diagnostic Commands

```bash
# Check if tools are installed
which platformio
which arm-none-eabi-gcc
which openocd

# Check tool versions
platformio --version
arm-none-eabi-gcc --version
openocd --version

# Check file permissions
ls -la ./tools/verify-toolchain.sh
```

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