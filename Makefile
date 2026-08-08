##############################################################################
# AdaptivePWM — STM32CubeIDE-compatible Makefile
# Target: STM32F401RETx (NUCLEO-F401RE)
#
# Build:   make -j8
# Clean:   make clean
# Toolchain: arm-none-eabi-gcc on PATH, or set TOOLCHAIN_PREFIX / GCC_PATH
##############################################################################

TARGET   = AdaptivePWM
BUILD_DIR = build/cubeide

# Toolchain (override: make GCC_PATH=/path/to/bin)
GCC_PATH ?=
PREFIX   ?= arm-none-eabi-
ifdef GCC_PATH
CC  = $(GCC_PATH)/$(PREFIX)gcc
AS  = $(GCC_PATH)/$(PREFIX)gcc -x assembler-with-cpp
CP  = $(GCC_PATH)/$(PREFIX)objcopy
SZ  = $(GCC_PATH)/$(PREFIX)size
else
CC  = $(PREFIX)gcc
AS  = $(PREFIX)gcc -x assembler-with-cpp
CP  = $(PREFIX)objcopy
SZ  = $(PREFIX)size
endif

HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

# MCU flags
CPU   = -mcpu=cortex-m4
FPU   = -mfpu=fpv4-sp-d16
FLOAT = -mfloat-abi=hard
MCU   = $(CPU) -mthumb $(FPU) $(FLOAT)

# Defines
DEFS = -DUSE_HAL_DRIVER -DSTM32F401xE -DHSE_VALUE=16000000U

# Includes
INCLUDES = \
  -ICore/Inc \
  -IApp/Inc \
  -IApp/Inc/safety \
  -Iconfig \
  -IDrivers/STM32F4xx_HAL_Driver/Inc \
  -IDrivers/STM32F4xx_HAL_Driver/Inc/Legacy \
  -IDrivers/CMSIS/Device/ST/STM32F4xx/Include \
  -IDrivers/CMSIS/Include

# Application sources
APP_C = $(wildcard App/Src/*.c) $(wildcard App/Src/safety/*.c)
CORE_C = \
  Core/Src/system_stm32f4xx.c \
  Core/Src/stm32f4xx_hal_msp.c \
  Core/Src/syscalls.c \
  Core/Src/sysmem.c

HAL_C = \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc_ex.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_iwdg.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c

ASM_SOURCES = Startup/startup_stm32f401xe.s

C_SOURCES = $(APP_C) $(CORE_C) $(HAL_C)

LDSCRIPT = STM32F401RETX_FLASH.ld

CFLAGS = $(MCU) $(DEFS) $(INCLUDES) -Og -Wall -fdata-sections -ffunction-sections -g -gdwarf-2
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"

ASFLAGS = $(MCU) $(DEFS) $(INCLUDES) -Og -Wall -fdata-sections -ffunction-sections

LDFLAGS = $(MCU) -specs=nano.specs -T$(LDSCRIPT) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections
LIBS = -lc -lm -lnosys

OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

.PHONY: all clean size flash

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin size

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) $(LIBS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@

size: $(BUILD_DIR)/$(TARGET).elf
	$(SZ) $<

clean:
	rm -rf $(BUILD_DIR)

-include $(wildcard $(BUILD_DIR)/*.d)
