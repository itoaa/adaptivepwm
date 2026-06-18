#!/bin/bash

# Toolchain Integrity Verification Script
# SEC-133: AdaptivePWM Development Toolchain Verification
# Framework: CISSP D3/D8, NIST PR.DS-06, ISO A.8.8

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Exit handler
cleanup() {
    if [ $? -ne 0 ]; then
        log_error "Toolchain verification failed!"
        exit 1
    fi
}

trap cleanup EXIT

# Function to calculate SHA256 checksum
calculate_checksum() {
    local file="$1"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$file" | cut -d' ' -f1
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$file" | cut -d' ' -f1
    else
        log_error "No SHA256 checksum tool available"
        exit 1
    fi
}

# Function to verify binary against known checksum
verify_binary() {
    local binary_path="$1"
    local expected_checksum="$2"
    local tool_name="$3"
    
    # Check if file exists with ~ expansion
    if [ ! -f "$binary_path" ]; then
        local expanded_path="${binary_path/#\~/$HOME}"
        if [ -f "$expanded_path" ]; then
            binary_path="$expanded_path"
        else
            log_error "$tool_name binary not found at $binary_path"
            return 1
        fi
    fi
    
    local actual_checksum
    actual_checksum=$(calculate_checksum "$binary_path")
    
    if [ "$actual_checksum" = "$expected_checksum" ]; then
        log_success "$tool_name verified (SHA256: $actual_checksum)"
        return 0
    else
        log_error "$tool_name checksum mismatch!"
        log_error "Expected: $expected_checksum"
        log_error "Actual:   $actual_checksum"
        # Provide additional context for debugging
        if [ "$VERBOSE" = "true" ]; then
            log_info "File size: $(stat -c%s "$binary_path" 2>/dev/null || echo 'unknown') bytes"
            log_info "File permissions: $(stat -c%A "$binary_path" 2>/dev/null || echo 'unknown')"
        fi
        return 1
    fi
}

# Function to get tool version
get_tool_version() {
    local binary_path="$1"
    local version_flag="$2"
    
    if [ -f "$binary_path" ]; then
        "$binary_path" "$version_flag" 2>&1 | head -1 || echo "Unknown version"
    elif [ -f "${binary_path/#\~/$HOME}" ]; then
        # Try with home directory expansion
        "${binary_path/#\~/$HOME}" "$version_flag" 2>&1 | head -1 || echo "Unknown version"
    elif command -v "$(basename "$binary_path")" >/dev/null 2>&1; then
        "$(basename "$binary_path")" "$version_flag" 2>&1 | head -1 || echo "Unknown version"
    else
        echo "Not installed"
    fi
}

# Main verification function
main() {
    log_info "Starting toolchain integrity verification..."
    
    # Get script directory
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
    
    # Create build-attestations directory if it doesn't exist
    mkdir -p "$PROJECT_ROOT/build-attestations"
    
    # Check if we can write to the build-attestations directory
    if [ ! -w "$PROJECT_ROOT/build-attestations" ]; then
        log_error "Cannot write to build-attestations directory"
        exit 1
    fi
    
    # Define toolchain components and their expected checksums
    # These are known-good checksums for the specific tool versions we trust
    declare -A TOOLCHAIN_COMPONENTS=(
        # PlatformIO Core 6.1.19
        ["/home/linuxbrew/.linuxbrew/bin/platformio"]="20517772d7343ad1387410c1344a6adc9c18c7c3cc5dba9036ac0ffa052c46ba"
        
        # GCC ARM None EABI 12.3.1 20230626
        ["/home/ola/.platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-gcc"]="6b2d82f129c723fb1fe353fa308251aea7e7f1054b84d1dbb2065f34ed944f77"
        
        # OpenOCD 0.12.0-01004-g9ea7f3d64-dirty
        ["/home/ola/.platformio/packages/tool-openocd/bin/openocd"]="924370bd1db2c88b5ad3669a8b8d93b4bcdd87c4b9574dbed0635d0b9c252eb5"
    )
    
    # Verify each component
    local verification_failed=0
    
    for binary_path in "${!TOOLCHAIN_COMPONENTS[@]}"; do
        expected_checksum="${TOOLCHAIN_COMPONENTS[$binary_path]}"
        tool_name=$(basename "$binary_path")
        
        # Skip verification if binary path is empty
        if [ -z "$binary_path" ]; then
            continue
        fi
        
        if ! verify_binary "$binary_path" "$expected_checksum" "$tool_name"; then
            verification_failed=1
        fi
    done
    
    # Display tool versions
    log_info "Toolchain versions:"
    local platformio_path="/home/linuxbrew/.linuxbrew/bin/platformio"
    local gcc_path="/home/ola/.platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-gcc"
    local openocd_path="/home/ola/.platformio/packages/tool-openocd/bin/openocd"
    
    echo "  PlatformIO: $(get_tool_version "$platformio_path" --version)"
    echo "  GCC ARM: $(get_tool_version "$gcc_path" --version)"
    echo "  OpenOCD: $(get_tool_version "$openocd_path" --version)"
    
    log_info "Verification completed with status: $([ $verification_failed -eq 0 ] && echo "PASSED" || echo "FAILED")"
    
    # Create attestation report
    local timestamp
    timestamp=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
    
    cat > "$PROJECT_ROOT/build-attestations/toolchain-verification-$timestamp.json" << EOF
{
  "tool": "toolchain-verification",
  "version": "1.0",
  "timestamp": "$timestamp",
  "components": {
    "platformio": {
      "path": "$platformio_path",
      "checksum": "$(calculate_checksum "$platformio_path" 2>/dev/null || echo 'NOT_INSTALLED')",
      "version": "$(get_tool_version "$platformio_path" --version)"
    },
    "gcc-arm-none-eabi": {
      "path": "$gcc_path",
      "checksum": "$(calculate_checksum "$gcc_path" 2>/dev/null || echo 'NOT_INSTALLED')",
      "version": "$(get_tool_version "$gcc_path" --version)"
    },
    "openocd": {
      "path": "$openocd_path",
      "checksum": "$(calculate_checksum "$openocd_path" 2>/dev/null || echo 'NOT_INSTALLED')",
      "version": "$(get_tool_version "$openocd_path" --version)"
    }
  },
  "verification_status": "$([ $verification_failed -eq 0 ] && echo "PASSED" || echo "FAILED")",
  "strict_mode": "$([ "$STRICT_MODE" = "true" ] && echo "true" || echo "false")"
}
EOF
    
    if [ $verification_failed -eq 0 ]; then
        log_success "All toolchain components verified successfully"
        log_info "Attestation report saved to $PROJECT_ROOT/build-attestations/toolchain-verification-$timestamp.json"
        exit 0
    else
        log_error "Toolchain verification failed"
        exit 1
    fi
}

# Parse command line arguments
STRICT_MODE=false
VERBOSE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --strict)
            STRICT_MODE=true
            shift
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        --help|-h)
            echo "Toolchain Integrity Verification Script"
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --strict    Enable strict mode (fail on any checksum mismatch)"
            echo "  --verbose   Enable verbose output"
            echo "  --help, -h  Show this help message"
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Run main function
main