# STM32CubeIDE layout

This repository is structured as an **STM32CubeIDE / Makefile** project for **NUCLEO-F401RE** (STM32F401RETx).

## Tree

```
AdaptivePWM/
├── App/                 # Application firmware (your code)
│   ├── Inc/
│   └── Src/
├── Core/                # Cube-style system files (HAL conf, MSP, syscalls)
│   ├── Inc/
│   └── Src/
├── Drivers/             # CMSIS + STM32F4 HAL (vendored subset)
├── Startup/             # startup_stm32f401xe.s
├── config/              # features.h (bring-up / security switches)
├── STM32F401RETX_FLASH.ld
├── Makefile             # primary build (CubeIDE can import as Makefile project)
└── .project             # Eclipse / CubeIDE project name
```

Application code lives only under **`App/`** (not a parallel `src/` tree).

## Build (CLI)

```bash
# Requires arm-none-eabi-gcc and make on PATH
make -j$(nproc)

# Or point at a toolchain:
make GCC_PATH=/path/to/arm-gnu-toolchain/bin -j8
```

Outputs: `build/cubeide/AdaptivePWM.{elf,bin,hex,map}`

## Open in STM32CubeIDE

1. **File → Import → C/C++ → Existing Code as Makefile Project**
2. Root = this repository, Toolchain = **ARM Cross GCC** (or MCU ARM GCC)
3. Or **File → Open Projects from File System** and select the folder with `.project`
4. Build with the Makefile target, or run `make` in the project root

Debug: create a debug configuration for **STM32F401RE**, ST-Link SWD, load `build/cubeide/AdaptivePWM.elf`.

## Application entry

- `App/Src/main.c` — clock, init, bare-metal superloop
- Feature flags: `config/features.h`
- HAL peripheral setup lives in `App/Src/hal_*.c` (not CubeMX-generated MSP)

## Note on CubeMX

A full `.ioc` can be added later for pinout UI. Current clock/pins match the previous PlatformIO design (HSE 16 MHz → 84 MHz, TIM1 PWM, ADC1 DMA, USART2, IWDG).
