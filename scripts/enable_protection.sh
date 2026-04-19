#!/bin/bash
#
# Production Flash Script for AdaptivePWM Secure Boot
# Enables RDP Level 2 and disables SWD debug interface
#
# WARNING: This script performs IRREVERSIBLE operations!
# - RDP Level 2 cannot be disabled
# - SWD debug interface will be permanently disabled
# - Mass erase will be disabled
#
# Usage:
#   ./enable_protection.sh [--dry-run] [--yes-i-am-sure]
#
# Requirements:
#   - ST-Link or compatible programmer
#   - OpenOCD installed
#   - Bootloader already flashed and verified
#

set -euo pipefail

# Colors for output
RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

# Configuration
OPENOCD_BIN="${OPENOCD_BIN:-openocd}"
OPENOCD_INTERFACE="${OPENOCD_INTERFACE:-interface/stlink.cfg}"
OPENOCD_TARGET="${OPENOCD_TARGET:-target/stm32f4x.cfg}"
BOOTLOADER_BIN="${BOOTLOADER_BIN:-../bootloader/build/secure_bootloader.bin}"

# Flags
DRY_RUN=false
CONFIRMED=false

# Print warning banner
print_warning() {
    echo -e "${RED}"
    echo "╔══════════════════════════════════════════════════════════════════════╗"
    echo "║                    ⚠️  CRITICAL WARNING  ⚠️                           ║"
    echo "╠══════════════════════════════════════════════════════════════════════╣"
    echo "║                                                                      ║"
    echo "║  This script will enable RDP Level 2 on your STM32F401 device.       ║"
    echo "║                                                                      ║"
    echo "║  CONSEQUENCES OF RDP LEVEL 2:                                        ║"
    echo "║  ✗ Debug interface (SWD/JTAG) will be PERMANENTLY disabled          ║"
    echo "║  ✗ Flash cannot be read externally                                  ║"
    echo "║  ✗ Mass erase command will be disabled                              ║"
    echo "║  ✗ No way to recover or reset the device via debug                  ║"
    echo "║                                                                      ║"
    echo "║  REQUIREMENTS BEFORE PROCEEDING:                                      ║"
    echo "║  ✓ Bootloader is tested and working                                  ║"
    echo "║  ✓ Secure boot verifies signatures correctly                         ║"
    echo "║  ✓ Recovery mode is functional                                       ║"
    echo "║  ✓ Production firmware is signed and tested                          ║"
    echo "║  ✓ Hardware is ready for deployment                                  ║"
    echo "║                                                                      ║"
    echo "║  This operation is IRREVERSIBLE.                                     ║"
    echo "╚══════════════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

# Parse arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --dry-run)
                DRY_RUN=true
                echo -e "${YELLOW}DRY RUN MODE - No changes will be made${NC}"
                shift
                ;;
            --yes-i-am-sure)
                CONFIRMED=true
                shift
                ;;
            --help|-h)
                echo "Usage: $0 [--dry-run] [--yes-i-am-sure]"
                echo ""
                echo "Options:"
                echo "  --dry-run         Show what would be done without making changes"
                echo "  --yes-i-am-sure   Skip confirmation prompts (use with caution!)"
                echo "  --help, -h        Show this help message"
                exit 0
                ;;
            *)
                echo "Unknown option: $1"
                echo "Use --help for usage information"
                exit 1
                ;;
        esac
    done
}

# Check prerequisites
check_prerequisites() {
    echo "Checking prerequisites..."
    
    # Check OpenOCD
    if ! command -v "$OPENOCD_BIN" &> /dev/null; then
        echo -e "${RED}Error: OpenOCD not found. Install with: sudo apt install openocd${NC}"
        exit 1
    fi
    echo "  ✓ OpenOCD found: $($OPENOCD_BIN --version | head -1)"
    
    # Check bootloader binary exists
    if [[ ! -f "$BOOTLOADER_BIN" ]]; then
        echo -e "${RED}Error: Bootloader binary not found: $BOOTLOADER_BIN${NC}"
        echo "Build bootloader first: cd ../bootloader && make"
        exit 1
    fi
    echo "  ✓ Bootloader binary found: $BOOTLOADER_BIN"
    
    # Check ST-Link connection
    echo "  Checking ST-Link connection..."
    if ! $OPENOCD_BIN -f "$OPENOCD_INTERFACE" -f "$OPENOCD_TARGET" \
        -c "init" -c "echo \"ST-Link detected\"" -c "exit" 2>/dev/null | grep -q "ST-Link detected"; then
        echo -e "${RED}Error: Cannot connect to ST-Link. Check connections.${NC}"
        exit 1
    fi
    echo "  ✓ ST-Link connected"
    
    echo ""
}

# Flash bootloader
flash_bootloader() {
    echo "Flashing bootloader..."
    
    if [[ "$DRY_RUN" == true ]]; then
        echo "  [DRY RUN] Would flash: $BOOTLOADER_BIN"
        return
    fi
    
    $OPENOCD_BIN -f "$OPENOCD_INTERFACE" -f "$OPENOCD_TARGET" \
        -c "init" \
        -c "reset halt" \
        -c "flash write_image erase $BOOTLOADER_BIN 0x08000000" \
        -c "verify_image $BOOTLOADER_BIN 0x08000000" \
        -c "reset run" \
        -c "exit"
    
    echo -e "  ${GREEN}✓ Bootloader flashed successfully${NC}"
    echo ""
}

# Check current RDP level
check_rdp_level() {
    echo "Checking current RDP level..."
    
    local rdp_output
    rdp_output=$($OPENOCD_BIN -f "$OPENOCD_INTERFACE" -f "$OPENOCD_TARGET" \
        -c "init" \
        -c "flash info 0" \
        -c "exit" 2>&1)
    
    # Parse RDP level from output
    if echo "$rdp_output" | grep -q "0xAA"; then
        echo "  Current RDP Level: 0 (No protection)"
        return 0
    elif echo "$rdp_output" | grep -q "0x55"; then
        echo "  Current RDP Level: 1 (Read protection)"
        return 1
    elif echo "$rdp_output" | grep -q "0xCC"; then
        echo -e "  ${GREEN}Current RDP Level: 2 (Full protection - already enabled)${NC}"
        return 2
    fi
    
    echo "  Warning: Could not determine RDP level"
    return -1
}

# Enable RDP Level 1 (intermediate step)
enable_rdp1() {
    echo "Enabling RDP Level 1 (read protection)..."
    
    if [[ "$DRY_RUN" == true ]]; then
        echo "  [DRY RUN] Would enable RDP Level 1"
        return
    fi
    
    $OPENOCD_BIN -f "$OPENOCD_INTERFACE" -f "$OPENOCD_TARGET" \
        -c "init" \
        -c "reset halt" \
        -c "stm32f4x option_write 0 0x1FFF 0xCCFFEFFF 0xFFE1FFDC" \
        -c "reset run" \
        -c "exit"
    
    echo -e "  ${GREEN}✓ RDP Level 1 enabled${NC}"
    echo ""
}

# Enable RDP Level 2 (FINAL STEP - IRREVERSIBLE)
enable_rdp2() {
    echo -e "${RED}"
    echo "╔══════════════════════════════════════════════════════════════════════╗"
    echo "║                    FINAL CONFIRMATION REQUIRED                       ║"
    echo "╚══════════════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
    
    if [[ "$CONFIRMED" != true ]]; then
        echo -e "${YELLOW}You are about to enable RDP Level 2. This is IRREVERSIBLE.${NC}"
        echo ""
        read -p "Type 'ENABLE RDP LEVEL 2' to proceed: " confirm
        
        if [[ "$confirm" != "ENABLE RDP LEVEL 2" ]]; then
            echo "Confirmation failed. Aborting."
            exit 1
        fi
        
        echo ""
        read -p "Are you ABSOLUTELY SURE? Type 'YES' to confirm: " confirm2
        
        if [[ "$confirm2" != "YES" ]]; then
            echo "Confirmation failed. Aborting."
            exit 1
        fi
    fi
    
    echo ""
    echo -e "${RED}Enabling RDP Level 2 (FULL PROTECTION)...${NC}"
    
    if [[ "$DRY_RUN" == true ]]; then
        echo "  [DRY RUN] Would enable RDP Level 2"
        echo "  [DRY RUN] Would set option bytes to 0xCCFF..."
        return
    fi
    
    # Enable RDP Level 2
    $OPENOCD_BIN -f "$OPENOCD_INTERFACE" -f "$OPENOCD_TARGET" \
        -c "init" \
        -c "reset halt" \
        -c "stm32f4x option_write 0 0x1FFF 0xCCFFCFFF 0xFFE1FFDC" \
        -c "reset run" \
        -c "exit"
    
    echo ""
    echo -e "${GREEN}╔══════════════════════════════════════════════════════════════════════╗"
    echo "║               RDP LEVEL 2 ENABLED SUCCESSFULLY                       ║"
    echo "╚══════════════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo "  ✓ Flash read protection: ACTIVE"
    echo "  ✓ SWD debug interface: DISABLED"
    echo "  ✓ Mass erase: DISABLED"
    echo "  ✓ Bootloader: PROTECTED"
    echo ""
    echo -e "${YELLOW}IMPORTANT:${NC}"
    echo "  - Debug interface is now PERMANENTLY disabled"
    echo "  - Device can only be updated via bootloader recovery mode"
    echo "  - Keep signed firmware backups for future updates"
    echo ""
}

# Verify protection status
verify_protection() {
    echo "Verifying protection status..."
    
    if [[ "$DRY_RUN" == true ]]; then
        echo "  [DRY RUN] Would verify protection"
        return
    fi
    
    # Try to connect - should fail if RDP Level 2 is enabled
    local verify_output
    verify_output=$($OPENOCD_BIN -f "$OPENOCD_INTERFACE" -f "$OPENOCD_TARGET" \
        -c "init" \
        -c "exit" 2>&1 || true)
    
    if echo "$verify_output" | grep -q "Error\|failed\|cannot"; then
        echo -e "  ${GREEN}✓ Debug interface is protected (expected for RDP Level 2)${NC}"
    else
        echo -e "  ${YELLOW}Warning: Debug interface still accessible${NC}"
        echo "  Protection may not be fully active yet"
    fi
    
    echo ""
}

# Main execution
main() {
    echo "AdaptivePWM Production Protection Script"
    echo "========================================"
    echo ""
    
    parse_args "$@"
    print_warning
    
    if [[ "$CONFIRMED" != true ]]; then
        read -p "Do you want to proceed? (yes/no): " proceed
        if [[ "$proceed" != "yes" ]]; then
            echo "Aborted by user."
            exit 0
        fi
    fi
    
    echo ""
    check_prerequisites
    
    # Step 1: Flash bootloader
    flash_bootloader
    
    # Step 2: Check current RDP level
    local current_rdp
    current_rdp=$(check_rdp_level)
    current_rdp=$?
    
    if [[ $current_rdp -eq 2 ]]; then
        echo -e "${GREEN}RDP Level 2 is already enabled. Nothing to do.${NC}"
        exit 0
    fi
    
    # Step 3: Enable RDP Level 1 first (optional intermediate step)
    if [[ $current_rdp -eq 0 ]]; then
        echo "Current RDP Level: 0"
        read -p "Enable RDP Level 1 first? Recommended for testing. (yes/no): " enable_rdp1_first
        if [[ "$enable_rdp1_first" == "yes" ]]; then
            enable_rdp1
        fi
    fi
    
    # Step 4: Enable RDP Level 2
    enable_rdp2
    
    # Step 5: Verify
    verify_protection
    
    echo -e "${GREEN}Production protection setup complete!${NC}"
    echo ""
    echo "Next steps:"
    echo "  1. Flash signed firmware via bootloader recovery mode"
    echo "  2. Test secure boot verification"
    echo "  3. Document device serial number for tracking"
    echo ""
}

# Run main function
main "$@"
