# Secure Bootloader Makefile for AdaptivePWM
# STM32F401 Nucleo-64
#
# Build configuration for secure bootloader

# Project name
PROJECT = secure_bootloader

# Toolchain
PREFIX = arm-none-eabi-
CC = $(PREFIX)gcc
CXX = $(PREFIX)g++
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
AR = $(PREFIX)ar
LD = $(PREFIX)gcc
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

# MCU
MCU = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

# Defines
DEFS = -DSTM32F401xE -DUSE_HAL_DRIVER -DARM_MATH_CM4

# Include paths
INCLUDES = \
    -I../include \
    -I$(CMSIS)/Include \
    -I$(HAL)/Inc \
    -I$(HAL)/Inc/Legacy

# CMSIS and HAL paths (adjust to your setup)
CMSIS = ../lib/CMSIS
HAL = ../lib/STM32F4xx_HAL_Driver

# Source files
C_SOURCES = \
    secure_bootloader.c \
    $(HAL)/Src/stm32f4xx_hal.c \
    $(HAL)/Src/stm32f4xx_hal_cortex.c \
    $(HAL)/Src/stm32f4xx_hal_gpio.c \
    $(HAL)/Src/stm32f4xx_hal_rcc.c \
    $(HAL)/Src/stm32f4xx_hal_uart.c \
    $(HAL)/Src/stm32f4xx_hal_flash.c \
    $(HAL)/Src/stm32f4xx_hal_flash_ex.c \
    $(CMSIS)/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c

ASM_SOURCES = \
    $(CMSIS)/Device/ST/STM32F4xx/Source/Templates/gcc/startup_stm32f401xe.s

# Object files
OBJECTS = $(C_SOURCES:.c=.o) $(ASM_SOURCES:.s=.o)

# Compiler flags
CFLAGS = $(MCU) $(DEFS) $(INCLUDES) \
    -O2 \
    -Wall -Wextra -Wpedantic \
    -fdata-sections -ffunction-sections \
    -fstack-protector-strong \
    --specs=nano.specs \
    -D__FPU_USED=1U

# C++ specific flags
CXXFLAGS = $(CFLAGS) \
    -fno-rtti -fno-exceptions

# Assembler flags
ASFLAGS = $(MCU) $(INCLUDES) \
    -Wall -fdata-sections -ffunction-sections

# Linker flags
LDFLAGS = $(MCU) \
    -specs=nosys.specs \
    -Tldscripts/secure_bootloader.ld \
    -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map,--cref \
    -Wl,--gc-sections \
    -static \
    -lc -lm -lstdc++ -lsupc++

# Build directory
BUILD_DIR = build

# Target
TARGET = $(BUILD_DIR)/$(PROJECT)

# =============================================================================
# BUILD RULES
# =============================================================================

.PHONY: all clean flash debug size disasm

# Default target
all: $(BUILD_DIR) $(TARGET).elf $(TARGET).bin $(TARGET).hex size

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile C files
%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

# Compile assembly files
%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) -c $(ASFLAGS) $< -o $@

# Link
$(TARGET).elf: $(OBJECTS)
	$(LD) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

# Create binary
$(TARGET).bin: $(TARGET).elf
	$(BIN) $< $@

# Create hex
$(TARGET).hex: $(TARGET).elf
	$(HEX) $< $@

# Print size
size: $(TARGET).elf
	@echo "============================================"
	@echo "Bootloader Size Report"
	@echo "============================================"
	$(SZ) -A -x $<
	@echo "============================================"
	$(SZ) -B $<

# Flash to device
flash: $(TARGET).bin
	st-flash write $< 0x08000000

# Debug with GDB
debug: $(TARGET).elf
	arm-none-eabi-gdb -ex "target extended-remote :3333" \
	                  -ex "monitor reset halt" \
	                  -ex "load" \
	                  $(TARGET).elf

# Generate disassembly
disasm: $(TARGET).elf
	arm-none-eabi-objdump -d $< > $(BUILD_DIR)/disassembly.txt

# Clean build
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(C_SOURCES:.c=.o)
	rm -f $(ASM_SOURCES:.s=.o)
	@echo "Clean complete"

# =============================================================================
# SECURITY TARGETS
# =============================================================================

# Sign firmware (requires tools/sign_firmware.py)
sign: $(TARGET).bin
	@echo "Signing firmware..."
	python3 ../tools/sign_firmware.py \
		$(TARGET).bin \
		$(TARGET)_signed.bin \
		../keys/bootloader_private.pem
	@echo "Signed firmware: $(TARGET)_signed.bin"

# Verify signature
verify: $(TARGET)_signed.bin
	@echo "Verifying signature..."
	python3 ../tools/verify_signature.py \
		$(TARGET)_signed.bin \
		../keys/bootloader_public.pem

# Set RDP Level 2 (PRODUCTION - IRREVERSIBLE!)
set-rdp2:
	@echo "WARNING: This will disable debug interface permanently!"
	@echo "Press any key to continue or Ctrl+C to abort..."
	@read -n 1
	st-flash --reset-write --flash-protect=2

# Set RDP Level 1 (READ PROTECTION)
set-rdp1:
	@echo "Setting RDP Level 1..."
	st-flash --reset-write --flash-protect=1

# Remove RDP (Level 0) - ERASES FLASH
remove-rdp:
	@echo "WARNING: This will ERASE all flash!"
	@echo "Press any key to continue or Ctrl+C to abort..."
	@read -n 1
	st-flash --reset-write --flash-protect=0

# =============================================================================
# HELP
# =============================================================================

help:
	@echo "Secure Bootloader Build System"
	@echo "==============================="
	@echo ""
	@echo "Build targets:"
	@echo "  make          - Build bootloader"
	@echo "  make clean    - Clean build files"
	@echo "  make flash    - Flash to STM32F401"
	@echo "  make debug    - Start GDB session"
	@echo "  make size     - Show size report"
	@echo "  make disasm   - Generate disassembly"
	@echo ""
	@echo "Security targets:"
	@echo "  make sign       - Sign firmware with Ed25519"
	@echo "  make verify     - Verify firmware signature"
	@echo "  make set-rdp1   - Set RDP Level 1 (read protection)"
	@echo "  make set-rdp2   - Set RDP Level 2 (PRODUCTION - IRREVERSIBLE!)"
	@echo "  make remove-rdp - Remove RDP (ERASES FLASH!)"
	@echo ""
	@echo "Security Warnings:"
	@echo "  - RDP Level 2 disables debug interface permanently"
	@echo "  - Private keys must be kept secure"
	@echo "  - Never commit keys to version control"
