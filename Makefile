.PHONY: all clean build flash monitor test docs

# Default target
all: build

# Build release version
build:
	platformio run -e nucleo_f401re

# Build debug version
build-debug:
	platformio run -e nucleo_f401re_debug

# Upload firmware
flash:
	platformio run --target upload

# Clean build artifacts
clean:
	platformio run --target clean
	-rm -rf .pio/build

# Monitor serial output
monitor:
	platformio device monitor -b 115200

# Run tests
test:
	cd test && python3 -m pytest test_adaptivepwm.py -v

# Generate documentation
docs:
	cd docs && mkdocs serve

# Static analysis
check:
	cppcheck --enable=all src/

# Format code
format:
	clang-format -i src/*.c src/*.h

# Size report
size:
	arm-none-eabi-size .pio/build/*/firmware.elf

# Disassembly
disasm:
	arm-none-eabi-objdump -d .pio/build/*/firmware.elf > firmware.lst

# Help
help:
	@echo "Available targets:"
	@echo "  build       - Build release version"
	@echo "  build-debug - Build debug version"
	@echo "  flash       - Upload to target"
	@echo "  clean       - Clean build files"
	@echo "  monitor     - Open serial monitor"
	@echo "  test        - Run unit tests"
	@echo "  docs        - Serve documentation"
	@echo "  check       - Run static analysis"
	@echo "  format      - Format source code"
	@echo "  size        - Show firmware size"
	@echo "  disasm      - Generate disassembly"
