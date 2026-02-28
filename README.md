# AdaptivePWM Optimizer

Adaptive PWM software for safe operation in buck/boost chargers/ESC. Real-time calculation of L/C/ESR, safe zones, and efficiency optimization.

## Features

- **Real-time Parameter Measurement**: Continuously measures inductance (L), capacitance (C), and equivalent series resistance (ESR)
- **Safety Constraints**: Implements hardware-safe limits for all operations
- **Efficiency Optimization**: Dynamically adjusts PWM duty cycle to maintain >95% efficiency
- **Error Handling**: Comprehensive error detection and safe failover mechanisms
- **Non-blocking Operation**: Uses non-blocking delays for responsive real-time control

## Hardware Requirements

- STM32 Nucleo-F401RE board (or compatible STM32F4 series)
- ADC capable sensors for voltage and current measurement
- PWM output capability for motor/charger control

## Installation

```bash
cd AdaptivePWM
pio lib install
pio run
```

## Configuration

Edit `platformio.ini` to configure for different STM32 boards:
```ini
[env:your_board]
platform = ststm32
board = your_board_name
framework = stm32cube
```

## Documentation

See `docs/` directory for detailed technical documentation including:
- Electrical parameter measurement algorithms
- Efficiency calculation methods
- Safety protocols and failover procedures
- Hardware integration guides

## Development Status

This is an improved version with enhanced safety features and proper error handling. The original version had critical safety issues that have been addressed in this implementation.

## License

MIT License - see LICENSE file for details.