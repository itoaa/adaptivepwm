# AdaptivePWM CLI Tool

Command Line Interface for controlling and monitoring the AdaptivePWM system with PKI-based security.

## Overview

The `pwmctl` tool provides secure command-line access to AdaptivePWM functionality, following the same security principles as the EJBCA-PKI project. It supports:

- Real-time system monitoring
- Configuration management
- Certificate-based authentication
- System diagnostics

## Installation

### Prerequisites

- OpenSSL development libraries
- GCC compiler
- GNU Make

On Ubuntu/Debian:
```bash
sudo apt-get install libssl-dev build-essential
```

### Building

```bash
cd cli
make
sudo make install
```

### Configuration

Create the configuration directory:
```bash
sudo make config
```

This creates `/etc/adaptivepwm/` with placeholder files for:
- `cert.pem` - Client certificate
- `key.pem` - Private key (permissions 600)
- `ca.pem` - CA certificate

## Usage

```bash
pwmctl [OPTIONS] COMMAND
```

### Commands

- `status` - Show current system status
- `configure` - Configure system parameters
- `monitor` - Monitor system in real-time
- `certinfo` - Show certificate information
- `diagnostics` - Run system diagnostics
- `help` - Show help message

### Options

- `-h, --help` - Show help message
- `-v, --verbose` - Enable verbose output
- `-s, --secure` - Enable PKI-based authentication
- `-e, --endpoint HOST` - Specify endpoint (default: localhost:8080)
- `-c, --cert FILE` - Certificate file
- `-k, --key FILE` - Private key file
- `-a, --ca FILE` - CA certificate file

### Examples

```bash
# Show system status
pwmctl status

# Monitor system with verbose output
pwmctl -v monitor

# Configure system with secure authentication
pwmctl -s configure

# Show certificate information
pwmctl -s certinfo

# Run diagnostics
pwmctl diagnostics
```

## Security Features

The CLI implements PKI-based authentication similar to the EJBCA-PKI project:

1. **Certificate-based Authentication**: Uses X.509 certificates for secure authentication
2. **Secure Communication**: All commands can be executed over encrypted channels
3. **Configuration Protection**: Sensitive configuration files have restricted permissions
4. **Certificate Validation**: Validates certificate chains against trusted CAs

## Integration with EJBCA-PKI

This CLI follows the same security model as your EJBCA-PKI project:

- Hybrid certificate support (traditional + PQC algorithms)
- Standardized certificate management
- Secure key storage
- Role-based access control (through certificate attributes)

Certificates for the CLI can be issued by your EJBCA-PKI infrastructure using the same scripts and procedures.

## Directory Structure

```
/etc/adaptivepwm/
├── cert.pem    # Client certificate
├── key.pem     # Private key (600 permissions)
└── ca.pem      # CA certificate
```

## Troubleshooting

### Certificate Issues

If you encounter certificate errors:
1. Verify certificate validity dates
2. Check certificate chain integrity
3. Ensure CA certificate is correctly installed
4. Verify private key permissions (should be 600)

### Connection Problems

If connection to the AdaptivePWM system fails:
1. Check endpoint configuration
2. Verify network connectivity
3. Confirm system is running
4. Check firewall settings

## Development

### Adding New Commands

To add new commands:

1. Add command type to `command_type_t` enum
2. Add command parsing in `parse_arguments`
3. Implement command function
4. Register command in main switch statement

### Extending Security Features

The security framework can be extended to support:
- Certificate revocation checking
- OCSP validation
- Certificate pinning
- Mutual TLS authentication

## License

MIT License - see LICENSE file for details.