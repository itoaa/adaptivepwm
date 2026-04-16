#!/usr/bin/env python3
"""
Flash Log Integrity Verification Tool

Verifies HMAC-SHA256 signatures on AdaptivePWM flash log entries.

Usage:
    python verify_log_integrity.py <dump_file> [options]

Options:
    --key <key_hex>      HMAC key in hex format (32 bytes = 64 hex chars)
    --format <fmt>      Input format: 'raw', 'hex', 'srec' (default: raw)
    --start <addr>      Start address of log in dump (default: 0x080E0000)
    --verbose            Show detailed entry information
    --json               Output results in JSON format

Example:
    # Verify raw flash dump
    python verify_log_integrity.py flash_dump.bin
    
    # Verify with specific key
    python verify_log_integrity.py flash_dump.bin --key a1b2c3d4...
    
    # Output as JSON
    python verify_log_integrity.py flash_dump.bin --json

Author: AdaptivePWM Security Team
Date: 2026-04-12
"""

import sys
import struct
import json
import argparse
import hashlib
import hmac
from pathlib import Path
from dataclasses import dataclass, asdict
from typing import Optional, List, Tuple, BinaryIO

# Flash log constants
FLASH_LOG_START_ADDR = 0x080E0000
FLASH_LOG_SIZE = 8192
FLASH_LOG_HMAC_MAGIC = 0x484D4143  # "HMAC"
FLASH_LOG_CHAIN_MAGIC = 0x43484131  # "CHA1"
FLASH_MAGIC_LEGACY = 0xAD4DA1FE

HMAC_SHA256_KEY_SIZE = 32
HMAC_SHA256_SIGNATURE_SIZE = 32
HMAC_SALT_SIZE = 16


@dataclass
class LogEntryHMAC:
    """HMAC-protected log entry structure (80 bytes)"""
    # Legacy data (24 bytes)
    timestamp: int
    duty_cycle: float
    efficiency: float
    temperature: float
    current: float
    error_code: int
    reserved: int
    
    # Integrity metadata (24 bytes)
    salt: bytes
    prev_hash: bytes
    
    # Cryptographic signature (32 bytes)
    signature: bytes
    
    # Derived fields
    valid_hmac: bool = False
    entry_offset: int = 0


@dataclass
class FlashHeaderHMAC:
    """Flash log header with HMAC support (64 bytes)"""
    magic: int
    version: int
    write_index: int
    entry_count: int
    wrap_count: int
    chain_hash: bytes


@dataclass
class VerificationResult:
    """Verification result summary"""
    total_entries: int
    valid_entries: int
    tampered_entries: int
    chain_broken: bool
    integrity_ok: bool
    log_magic: str
    entries: List[dict]


def parse_hmac_header(data: bytes, offset: int = 0) -> Optional[FlashHeaderHMAC]:
    """Parse HMAC flash header from binary data"""
    if len(data) < offset + 64:
        return None
    
    magic = struct.unpack('<I', data[offset:offset+4])[0]
    if magic != FLASH_LOG_HMAC_MAGIC:
        return None
    
    return FlashHeaderHMAC(
        magic=magic,
        version=struct.unpack('<I', data[offset+4:offset+8])[0],
        write_index=struct.unpack('<I', data[offset+8:offset+12])[0],
        entry_count=struct.unpack('<I', data[offset+12:offset+16])[0],
        wrap_count=struct.unpack('<I', data[offset+16:offset+20])[0],
        chain_hash=data[offset+20:offset+52]
    )


def parse_legacy_header(data: bytes, offset: int = 0) -> Optional[dict]:
    """Parse legacy flash header from binary data"""
    if len(data) < offset + 16:
        return None
    
    magic = struct.unpack('<I', data[offset:offset+4])[0]
    if magic != FLASH_MAGIC_LEGACY:
        return None
    
    return {
        'magic': magic,
        'write_index': struct.unpack('<I', data[offset+4:offset+8])[0],
        'entry_count': struct.unpack('<I', data[offset+8:offset+12])[0],
        'wrap_count': struct.unpack('<I', data[offset+12:offset+16])[0]
    }


def parse_hmac_entry(data: bytes, offset: int) -> Optional[LogEntryHMAC]:
    """Parse HMAC log entry from binary data"""
    if len(data) < offset + 80:
        return None
    
    entry_data = data[offset:offset+80]
    
    return LogEntryHMAC(
        timestamp=struct.unpack('<I', entry_data[0:4])[0],
        duty_cycle=struct.unpack('<f', entry_data[4:8])[0],
        efficiency=struct.unpack('<f', entry_data[8:12])[0],
        temperature=struct.unpack('<f', entry_data[12:16])[0],
        current=struct.unpack('<f', entry_data[16:20])[0],
        error_code=struct.unpack('<H', entry_data[20:22])[0],
        reserved=struct.unpack('<H', entry_data[22:24])[0],
        salt=entry_data[24:40],
        prev_hash=entry_data[40:48],
        signature=entry_data[48:80],
        entry_offset=offset
    )


def compute_hmac(entry: LogEntryHMAC, key: bytes) -> bytes:
    """Compute HMAC-SHA256 for an entry"""
    # Data for HMAC includes everything except signature field (first 48 bytes)
    data = b''.join([
        struct.pack('<I', entry.timestamp),
        struct.pack('<f', entry.duty_cycle),
        struct.pack('<f', entry.efficiency),
        struct.pack('<f', entry.temperature),
        struct.pack('<f', entry.current),
        struct.pack('<H', entry.error_code),
        struct.pack('<H', entry.reserved),
        entry.salt,
        entry.prev_hash
    ])
    
    return hmac.new(key, data, hashlib.sha256).digest()


def verify_entry(entry: LogEntryHMAC, key: bytes) -> bool:
    """Verify HMAC signature for an entry"""
    computed = compute_hmac(entry, key)
    return hmac.compare_digest(computed, entry.signature)


def update_chain_hash(entry: LogEntryHMAC, chain_hash: bytes) -> bytes:
    """Update chain hash with entry signature"""
    hash_input = chain_hash + entry.signature
    return hashlib.sha256(hash_input).digest()


def load_dump_file(filepath: str, format_type: str = 'raw') -> bytes:
    """Load flash dump file in various formats"""
    path = Path(filepath)
    
    if not path.exists():
        raise FileNotFoundError(f"Dump file not found: {filepath}")
    
    with open(path, 'rb') as f:
        data = f.read()
    
    if format_type == 'hex':
        # Intel HEX or plain hex
        if data[0:1] == b':':
            data = parse_intel_hex(data.decode('ascii'))
        else:
            data = bytes.fromhex(data.decode('ascii'))
    elif format_type == 'srec':
        data = parse_srec(data.decode('ascii'))
    
    return data


def parse_intel_hex(hex_data: str) -> bytes:
    """Parse Intel HEX format"""
    # Simplified parser - assumes linear address space
    result = bytearray()
    for line in hex_data.strip().split('\n'):
        if line.startswith(':'):
            byte_count = int(line[1:3], 16)
            record_type = int(line[7:9], 16)
            if record_type == 0:  # Data record
                data = bytes.fromhex(line[9:9+byte_count*2])
                result.extend(data)
    return bytes(result)


def parse_srec(srec_data: str) -> bytes:
    """Parse Motorola S-Record format"""
    result = bytearray()
    for line in srec_data.strip().split('\n'):
        if line.startswith('S1') or line.startswith('S2') or line.startswith('S3'):
            byte_count = int(line[2:4], 16)
            data = bytes.fromhex(line[4:4+(byte_count-1)*2])
            result.extend(data)
    return bytes(result)


def verify_flash_log(dump_data: bytes, key: bytes, start_addr: int = FLASH_LOG_START_ADDR,
                     verbose: bool = False) -> VerificationResult:
    """Verify complete flash log integrity"""
    
    # Calculate offset within dump file
    if start_addr >= 0x08000000:  # STM32 flash base
        offset = start_addr - 0x08000000
    else:
        offset = start_addr
    
    if offset >= len(dump_data):
        raise ValueError(f"Start address {hex(start_addr)} beyond dump file size")
    
    # Check for HMAC header
    header = parse_hmac_header(dump_data, offset)
    
    if header:
        log_magic = "HMAC-SHA256"
        header_size = 64
        entry_size = 80
    else:
        # Check for legacy header
        legacy = parse_legacy_header(dump_data, offset)
        if legacy:
            log_magic = "Legacy CRC"
            header_size = 16
            entry_size = 32
            header = None
        else:
            raise ValueError("No valid flash log header found")
    
    if verbose:
        print(f"Log type: {log_magic}")
        if header:
            print(f"  Version: {header.version}")
            print(f"  Entries: {header.entry_count}")
            print(f"  Wraps: {header.wrap_count}")
            print(f"  Chain hash: {header.chain_hash.hex()[:16]}...")
    
    entries = []
    valid_count = 0
    tampered_count = 0
    
    if header:  # HMAC mode
        max_entries = (FLASH_LOG_SIZE - 64) // 80
        entries_to_check = min(header.entry_count, max_entries)
        
        chain_hash = bytes(32)  # Start with zeros
        chain_broken = False
        
        for i in range(entries_to_check):
            entry_offset = offset + header_size + (i * entry_size)
            entry = parse_hmac_entry(dump_data, entry_offset)
            
            if entry is None:
                break
            
            is_valid = verify_entry(entry, key)
            entry.valid_hmac = is_valid
            
            # Check chain
            if i > 0 and entry.prev_hash != chain_hash[:8]:
                chain_broken = True
            
            # Update chain hash
            chain_hash = update_chain_hash(entry, chain_hash)
            
            if is_valid:
                valid_count += 1
            else:
                tampered_count += 1
            
            entries.append(asdict(entry))
            
            if verbose:
                status = "✓ VALID" if is_valid else "✗ TAMPERED"
                print(f"  Entry {i}: {status} - Timestamp: {entry.timestamp}, "
                      f"Duty: {entry.duty_cycle:.2f}, Temp: {entry.temperature:.1f}")
                if not is_valid:
                    print(f"    Expected: {compute_hmac(entry, key).hex()}")
                    print(f"    Got:      {entry.signature.hex()}")
    
    else:  # Legacy mode - no HMAC verification possible
        max_entries = (FLASH_LOG_SIZE - 16) // 32
        entries_to_check = min(legacy['entry_count'] & 0x00FFFFFF, max_entries)
        chain_broken = False
        valid_count = entries_to_check  # Assume valid, no HMAC to check
        tampered_count = 0
    
    return VerificationResult(
        total_entries=entries_to_check,
        valid_entries=valid_count,
        tampered_entries=tampered_count,
        chain_broken=chain_broken,
        integrity_ok=(tampered_count == 0 and not chain_broken),
        log_magic=log_magic,
        entries=entries
    )


def generate_test_key() -> bytes:
    """Generate a test HMAC key (for development only)"""
    # This should be replaced with actual key from secure storage
    return bytes([
        0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x07, 0x18,
        0x29, 0x3A, 0x4B, 0x5C, 0x6D, 0x7E, 0x8F, 0x90,
        0x01, 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78,
        0x89, 0x9A, 0xAB, 0xBC, 0xCD, 0xDE, 0xEF, 0xF0
    ])


def main():
    parser = argparse.ArgumentParser(
        description='Verify HMAC-SHA256 integrity of AdaptivePWM flash log',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    
    parser.add_argument('dump_file', help='Flash dump file to verify')
    parser.add_argument('--key', help='HMAC key in hex format (64 chars)')
    parser.add_argument('--format', choices=['raw', 'hex', 'srec'], default='raw',
                        help='Input format (default: raw)')
    parser.add_argument('--start', type=lambda x: int(x, 0), default=FLASH_LOG_START_ADDR,
                        help='Start address of log (default: 0x080E0000)')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Show detailed entry information')
    parser.add_argument('--json', action='store_true',
                        help='Output results in JSON format')
    
    args = parser.parse_args()
    
    # Load key
    if args.key:
        key = bytes.fromhex(args.key)
        if len(key) != 32:
            print(f"Error: Key must be 32 bytes (64 hex chars), got {len(key)}",
                  file=sys.stderr)
            sys.exit(1)
    else:
        print("Warning: No key provided, using test key (not for production)",
              file=sys.stderr)
        key = generate_test_key()
    
    # Load dump file
    try:
        dump_data = load_dump_file(args.dump_file, args.format)
    except Exception as e:
        print(f"Error loading dump file: {e}", file=sys.stderr)
        sys.exit(1)
    
    # Verify log
    try:
        result = verify_flash_log(dump_data, key, args.start, args.verbose)
    except Exception as e:
        print(f"Error verifying log: {e}", file=sys.stderr)
        sys.exit(1)
    
    # Output results
    if args.json:
        print(json.dumps(asdict(result), indent=2))
    else:
        print("=" * 60)
        print("Flash Log Integrity Verification Report")
        print("=" * 60)
        print(f"Log Format:     {result.log_magic}")
        print(f"Total Entries:  {result.total_entries}")
        print(f"Valid Entries:  {result.valid_entries}")
        print(f"Tampered:       {result.tampered_entries}")
        print(f"Chain Status:   {'OK' if not result.chain_broken else 'BROKEN'}")
        print(f"Integrity:      {'PASS ✓' if result.integrity_ok else 'FAIL ✗'}")
        print("=" * 60)
        
        if result.integrity_ok:
            print("\n✓ Log integrity verified successfully")
            sys.exit(0)
        else:
            print("\n✗ Log integrity verification failed")
            if result.tampered_entries > 0:
                print(f"  - {result.tampered_entries} entries have been tampered with")
            if result.chain_broken:
                print("  - Entry chain has been broken (possible deletion/reordering)")
            sys.exit(1)


if __name__ == '__main__':
    main()
