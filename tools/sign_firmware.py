#!/usr/bin/env python3
"""
Firmware Signing Tool for AdaptivePWM Secure Boot
Implements Ed25519 signature generation for firmware images

Usage:
    sign_firmware.py <input.bin> <output_signed.bin> <private_key.pem>
    sign_firmware.py --generate-keys --key-dir <directory>

Security Requirements:
    - Private key must be kept secure (HSM or offline storage)
    - Never commit private keys to version control
    - Use separate keys for development and production

Dependencies:
    pip install pynacl cryptography

Copyright (C) 2026 AdaptivePWM Project
SPDX-License-Identifier: MIT
"""

import os
import sys
import argparse
import struct
import hashlib
from pathlib import Path
from datetime import datetime
from typing import Optional, Tuple

try:
    import nacl.signing
    import nacl.encoding
    from nacl.exceptions import CryptoError
except ImportError:
    print("Error: pynacl not installed. Run: pip install pynacl")
    sys.exit(1)

try:
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric import ed25519
    from cryptography.hazmat.backends import default_backend
except ImportError:
    print("Warning: cryptography not installed. Using pynacl only.")


# Firmware header structure (128 bytes)
FIRMWARE_HEADER_FORMAT = "<III32s64s16s"  # Little-endian
FIRMWARE_HEADER_SIZE = 128
FIRMWARE_MAGIC = 0x57445041  # "ADPW" in little-endian ASCII


class FirmwareHeader:
    """Represents firmware header structure"""
    
    def __init__(self):
        self.magic: int = FIRMWARE_MAGIC
        self.version: int = 1
        self.firmware_size: int = 0
        self.hash: bytes = b'\x00' * 32
        self.signature: bytes = b'\x00' * 64
        self.reserved: bytes = b'\x00' * 16
    
    def to_bytes(self) -> bytes:
        """Serialize header to bytes"""
        return struct.pack(
            FIRMWARE_HEADER_FORMAT,
            self.magic,
            self.version,
            self.firmware_size,
            self.hash,
            self.signature,
            self.reserved
        )
    
    @classmethod
    def from_bytes(cls, data: bytes) -> 'FirmwareHeader':
        """Parse header from bytes"""
        if len(data) < FIRMWARE_HEADER_SIZE:
            raise ValueError(f"Header too small: {len(data)} bytes")
        
        header = cls()
        unpacked = struct.unpack(FIRMWARE_HEADER_FORMAT, data[:FIRMWARE_HEADER_SIZE])
        header.magic = unpacked[0]
        header.version = unpacked[1]
        header.firmware_size = unpacked[2]
        header.hash = unpacked[3]
        header.signature = unpacked[4]
        header.reserved = unpacked[5]
        return header
    
    def validate(self) -> bool:
        """Validate header fields"""
        if self.magic != FIRMWARE_MAGIC:
            print(f"Error: Invalid magic: 0x{self.magic:08X}")
            return False
        if self.version == 0:
            print("Error: Version cannot be zero")
            return False
        if len(self.hash) != 32:
            print("Error: Invalid hash length")
            return False
        if len(self.signature) != 64:
            print("Error: Invalid signature length")
            return False
        return True


class KeyManager:
    """Manages Ed25519 key generation and storage"""
    
    PRIVATE_KEY_FILE = "bootloader_private.pem"
    PUBLIC_KEY_FILE = "bootloader_public.pem"
    PUBLIC_KEY_C_FILE = "bootloader_public_key.c"
    
    @staticmethod
    def generate_keys(output_dir: Path) -> Tuple[Path, Path]:
        """Generate new Ed25519 key pair"""
        print("Generating Ed25519 key pair...")
        
        # Generate signing key
        signing_key = nacl.signing.SigningKey.generate()
        
        # Get verify key
        verify_key = signing_key.verify_key
        
        # Create output directory
        output_dir.mkdir(parents=True, exist_ok=True)
        
        # Save private key
        private_key_path = output_dir / KeyManager.PRIVATE_KEY_FILE
        with open(private_key_path, 'wb') as f:
            # Store as 32-byte seed
            f.write(signing_key.encode())
        
        # Set restrictive permissions on private key
        os.chmod(private_key_path, 0o600)
        print(f"  Private key saved: {private_key_path}")
        
        # Save public key
        public_key_path = output_dir / KeyManager.PUBLIC_KEY_FILE
        with open(public_key_path, 'wb') as f:
            f.write(verify_key.encode())
        print(f"  Public key saved: {public_key_path}")
        
        # Generate C header for embedding
        c_file_path = output_dir / KeyManager.PUBLIC_KEY_C_FILE
        KeyManager._generate_c_header(c_file_path, verify_key.encode())
        print(f"  C header saved: {c_file_path}")
        
        return private_key_path, public_key_path
    
    @staticmethod
    def _generate_c_header(output_path: Path, public_key: bytes):
        """Generate C header file with public key"""
        key_str = ', '.join(f'0x{b:02X}' for b in public_key)
        
        content = f"""/**
 * @file bootloader_public_key.c
 * @brief Embedded Ed25519 public key for firmware verification
 * @generated {datetime.now().isoformat()}
 * 
 * DO NOT EDIT - Generated by sign_firmware.py
 * This key is used by the secure bootloader to verify firmware signatures.
 */

#include <stdint.h>

/* Ed25519 public key - 32 bytes */
const uint8_t EMBEDDED_PUBLIC_KEY[32] = {{
    {key_str}
}};

/* Key hash for integrity verification */
const uint8_t PUBLIC_KEY_HASH[32] = {{
    /* SHA-256 hash of public key - verify at build time */
}};
"""
        with open(output_path, 'w') as f:
            f.write(content)
    
    @staticmethod
    def load_private_key(key_path: Path) -> nacl.signing.SigningKey:
        """Load private key from file"""
        with open(key_path, 'rb') as f:
            key_data = f.read()
        
        # Try loading as 32-byte seed first
        if len(key_data) == 32:
            return nacl.signing.SigningKey(key_data)
        
        # Try loading as PEM
        try:
            from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
            private_key = serialization.load_pem_private_key(key_data, password=None)
            return nacl.signing.SigningKey(private_key.private_bytes(
                encoding=serialization.Encoding.Raw,
                format=serialization.PrivateFormat.Raw,
                encryption_algorithm=serialization.NoEncryption()
            ))
        except Exception:
            pass
        
        raise ValueError(f"Unable to load private key from {key_path}")
    
    @staticmethod
    def load_public_key(key_path: Path) -> nacl.signing.VerifyKey:
        """Load public key from file"""
        with open(key_path, 'rb') as f:
            key_data = f.read()
        
        if len(key_data) == 32:
            return nacl.signing.VerifyKey(key_data)
        
        raise ValueError(f"Unable to load public key from {key_path}")


class FirmwareSigner:
    """Signs firmware images with Ed25519"""
    
    def __init__(self, private_key: nacl.signing.SigningKey):
        self.private_key = private_key
    
    def sign(self, firmware_data: bytes, version: int) -> bytes:
        """
        Sign firmware and create signed image
        
        Args:
            firmware_data: Raw firmware binary
            version: Firmware version (monotonically increasing)
        
        Returns:
            Signed firmware image (header + firmware)
        """
        # Create header
        header = FirmwareHeader()
        header.version = version
        header.firmware_size = len(firmware_data)
        
        # Calculate SHA-256 hash of firmware
        header.hash = hashlib.sha256(firmware_data).digest()
        
        # Create message to sign: hash || version || firmware_size
        message = (
            header.hash +
            struct.pack('<I', header.version) +
            struct.pack('<I', header.firmware_size)
        )
        
        # Sign message
        signed = self.private_key.sign(message)
        header.signature = signed.signature
        
        # Combine header and firmware
        return header.to_bytes() + firmware_data
    
    def verify(self, signed_data: bytes) -> bool:
        """
        Verify signed firmware
        
        Args:
            signed_data: Signed firmware image
        
        Returns:
            True if valid, False otherwise
        """
        if len(signed_data) < FIRMWARE_HEADER_SIZE:
            print("Error: Data too small for header")
            return False
        
        # Parse header
        header = FirmwareHeader.from_bytes(signed_data[:FIRMWARE_HEADER_SIZE])
        
        if not header.validate():
            return False
        
        # Extract firmware
        firmware = signed_data[FIRMWARE_HEADER_SIZE:]
        if len(firmware) != header.firmware_size:
            print(f"Error: Firmware size mismatch: {len(firmware)} != {header.firmware_size}")
            return False
        
        # Verify hash
        calculated_hash = hashlib.sha256(firmware).digest()
        if calculated_hash != header.hash:
            print("Error: Hash mismatch")
            return False
        
        # Verify signature
        message = (
            header.hash +
            struct.pack('<I', header.version) +
            struct.pack('<I', header.firmware_size)
        )
        
        try:
            verify_key = self.private_key.verify_key
            verify_key.verify(message, header.signature)
            print("Signature verified successfully")
            return True
        except nacl.exceptions.BadSignatureError:
            print("Error: Invalid signature")
            return False


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description="Sign firmware images for AdaptivePWM Secure Boot"
    )
    parser.add_argument('input', nargs='?', help='Input firmware binary')
    parser.add_argument('output', nargs='?', help='Output signed firmware')
    parser.add_argument('private_key', nargs='?', help='Private key file')
    parser.add_argument('--version', '-v', type=int, default=1,
                        help='Firmware version (default: 1)')
    parser.add_argument('--generate-keys', '-g', action='store_true',
                        help='Generate new key pair')
    parser.add_argument('--key-dir', type=Path, default=Path('../keys'),
                        help='Directory for keys (default: ../keys)')
    parser.add_argument('--verify', action='store_true',
                        help='Verify signed firmware')
    
    args = parser.parse_args()
    
    # Generate keys mode
    if args.generate_keys:
        private_path, public_path = KeyManager.generate_keys(args.key_dir)
        print(f"\nKeys generated successfully!")
        print(f"  Private: {private_path}")
        print(f"  Public:  {public_path}")
        print(f"\nIMPORTANT:")
        print(f"  1. Store the private key securely (HSM or offline)")
        print(f"  2. Never commit private keys to version control")
        print(f"  3. Backup private key in secure location")
        print(f"  4. Include the C header in bootloader build")
        return 0
    
    # Check required arguments
    if not args.input or not args.output or not args.private_key:
        parser.print_help()
        return 1
    
    # Resolve paths
    input_path = Path(args.input)
    output_path = Path(args.output)
    key_path = Path(args.private_key)
    
    # Validate input file
    if not input_path.exists():
        print(f"Error: Input file not found: {input_path}")
        return 1
    
    # Load private key
    try:
        private_key = KeyManager.load_private_key(key_path)
    except Exception as e:
        print(f"Error loading private key: {e}")
        return 1
    
    # Read firmware
    print(f"Reading firmware: {input_path}")
    with open(input_path, 'rb') as f:
        firmware = f.read()
    
    print(f"Firmware size: {len(firmware)} bytes")
    print(f"Version: {args.version}")
    
    # Sign firmware
    signer = FirmwareSigner(private_key)
    
    if args.verify:
        print("\nVerifying signed firmware...")
        signed_data = firmware  # In verify mode, input is already signed
        if signer.verify(signed_data):
            print("Verification PASSED")
            return 0
        else:
            print("Verification FAILED")
            return 1
    else:
        print("\nSigning firmware...")
        signed = signer.sign(firmware, args.version)
        
        # Write output
        output_path.parent.mkdir(parents=True, exist_ok=True)
        with open(output_path, 'wb') as f:
            f.write(signed)
        
        print(f"Signed firmware saved: {output_path}")
        print(f"Total size: {len(signed)} bytes (header: {FIRMWARE_HEADER_SIZE}, firmware: {len(firmware)})")
        
        # Verify immediately
        print("\nVerifying signature...")
        if signer.verify(signed):
            print("Self-verification PASSED")
            return 0
        else:
            print("Self-verification FAILED")
            return 1


if __name__ == "__main__":
    sys.exit(main())
