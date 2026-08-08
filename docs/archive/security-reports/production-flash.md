# AdaptivePWM Production Flashing Guide

## STM32 Option Bytes Configuration - Read Protection (RDP)

### Overview

This document describes how to enable RDP Level 2 (Read Protection) for production deployments of the AdaptivePWM firmware on STM32F4xx microcontrollers.

**WARNING: RDP Level 2 is IRREVERSIBLE. Once enabled, the chip cannot be debugged or reprogrammed via SWD/JTAG. This is the intended security behavior.**

### Protection Levels

| Level | Description | Debug Access | Firmware Read |
|-------|-------------|--------------|---------------|
| RDP 0 | No protection | Full access | Readable |
| RDP 1 | Read protection | No access | Protected |
| RDP 2 | Full protection | Disabled | Protected, irreversible |

### Prerequisites

- ST-Link programmer or compatible
- STM32CubeProgrammer or `st-flash` CLI tool
- Production firmware binary (verified and tested)
- Backup of encryption keys (if applicable)

### Enable RDP Level 2

#### Method 1: STM32CubeProgrammer (GUI)

1. Connect ST-Link to target board
2. Open STM32CubeProgrammer
3. Connect to target (SWD mode)
4. Navigate to **OB (Option Bytes)** tab
5. Set **Read Protection** to **Level 2**
6. Click **Apply** to write changes
7. Power cycle the device

#### Method 2: Command Line (st-flash)

```bash
# First, flash the production firmware
st-flash write adaptive_pwm_production.bin 0x08000000

# Enable RDP Level 2 (0x33 = Level 2)
st-flash --reset --connect-under-reset write 0x33 0x1FFF0000
```

#### Method 3: OpenOCD

```bash
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
    -c "init" \
    -c "reset halt" \
    -c "stm32f4x option_write 0 0x33" \
    -c "reset" \
    -c "exit"
```

### Verification

After enabling RDP Level 2:

1. **Debug interface check:**
   ```bash
   st-info --probe
   ```
   Should show: `Error: cannot read device id` or similar

2. **Firmware integrity:**
   - Device should boot normally
   - Application should function as expected
   - UART CLI should remain accessible (if enabled)

### Production Flash Script

Create `scripts/production_flash.sh`:

```bash
#!/bin/bash
set -e

FIRMWARE="$1"
if [ -z "$FIRMWARE" ]; then
    echo "Usage: $0 <firmware.bin>"
    exit 1
fi

echo "========================================"
echo "AdaptivePWM Production Flash Tool"
echo "WARNING: This will enable RDP Level 2"
echo "This operation is IRREVERSIBLE"
echo "========================================"
read -p "Are you sure? Type 'ENABLE-RDP2' to continue: " confirm

if [ "$confirm" != "ENABLE-RDP2" ]; then
    echo "Aborted."
    exit 1
fi

echo "[1/3] Flashing firmware..."
st-flash write "$FIRMWARE" 0x08000000

echo "[2/3] Verifying flash..."
st-flash --reset read /tmp/verify.bin $(stat -c%s "$FIRMWARE") 0x08000000
if ! diff "$FIRMWARE" /tmp/verify.bin; then
    echo "ERROR: Verification failed!"
    exit 1
fi
rm /tmp/verify.bin

echo "[3/3] Enabling RDP Level 2..."
st-flash --reset --connect-under-reset write 0x33 0x1FFF0000

echo "========================================"
echo "Production flash complete!"
echo "Device is now protected and ready for deployment."
echo "========================================"
```

### Recovery Procedure (Before RDP 2)

If RDP Level 1 is active, you can revert to Level 0:

```bash
# WARNING: This ERASES all flash memory
st-flash --reset --connect-under-reset write 0xAA 0x1FFF0000
```

**Note:** Once RDP Level 2 is enabled, there is NO recovery method.

### Security Considerations

1. **Key Storage:** Ensure any encryption keys are stored securely before enabling RDP 2
2. **Firmware Version:** Always flash the final, tested firmware version
3. **Batch Tracking:** Record chip serial numbers for production tracking
4. **Backup:** Maintain source code and build environment for future updates

### Related Documents

- STM32F4 Reference Manual (RM0368), Section 3.4: Read Protection
- AN2606: STM32 microcontroller system memory boot mode
- AN4701: STM32F4 Series security features

---
*Document Version: 1.0*  
*Last Updated: 2026-04-18*  
*Security Task: SEC-047*
