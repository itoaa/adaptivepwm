# AdaptivePWM Configuration Directory

## Overview

This directory contains platform-specific and build-specific configuration files for AdaptivePWM.

## Structure

```
config/
├── README.md              # This file
├── nucleo_f401re.h       # NUCLEO-F401RE board configuration
├── stm32f401xe.h          # STM32F401xE MCU configuration
└── debug.h                # Debug-specific overrides
```

## Configuration Files

### nucleo_f401re.h

Configuration specific to the NUCLEO-F401RE development board:
- Pin mappings
- Clock configuration
- Peripheral settings
- Safety thresholds

### stm32f401xe.h

Configuration specific to the STM32F401xE MCU family:
- Clock frequencies
- Peripheral capabilities
- Memory layout
- Power settings

### debug.h

Debug-specific configuration overrides:
- Extended logging
- Debug breakpoints
- Test mode settings
- Mock hardware settings

## Usage

Include the appropriate configuration header at compile time:

```c
// In your source file
#include "config/nucleo_f401re.h"
```

Or define at compile time:

```bash
-DCONFIG_BOARD_NUCLEO_F401RE
```

## Adding New Configurations

To add support for a new board or platform:

1. Create a new header file: `config/your_board.h`
2. Define all required macros
3. Update build system to include the new config
4. Document the configuration in this README

## Version Control

Configuration files should be version-controlled and reviewed when hardware changes occur.
