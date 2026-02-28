#
# Makefile for AdaptivePWM CLI
# Based on EJBCA-PKI project structure
#

# Compiler and linker settings
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
LDFLAGS = -lssl -lcrypto

# Directories
SRCDIR = .
OBJDIR = obj
BINDIR = bin

# Source and target files
SOURCES = pwmctl.c
TARGET = pwmctl
OBJECTS = $(SOURCES:%.c=$(OBJDIR)/%.o)

# Default target
all: directories $(BINDIR)/$(TARGET)

# Create necessary directories
directories:
	@mkdir -p $(OBJDIR) $(BINDIR)

# Compile source files
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Link object files
$(BINDIR)/$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

# Install to system
install: all
	install -d /usr/local/bin
	install -m 755 $(BINDIR)/$(TARGET) /usr/local/bin/

# Clean build files
clean:
	rm -rf $(OBJDIR) $(BINDIR)

# Create configuration directory
config:
	install -d /etc/adaptivepwm
	# Copy sample certificates if they exist
	@if [ -f sample-cert.pem ]; then \
		install -m 644 sample-cert.pem /etc/adaptivepwm/cert.pem; \
	fi
	@if [ -f sample-key.pem ]; then \
		install -m 600 sample-key.pem /etc/adaptivepwm/key.pem; \
	fi
	@if [ -f sample-ca.pem ]; then \
		install -m 644 sample-ca.pem /etc/adaptivepwm/ca.pem; \
	fi

# Help target
help:
	@echo "AdaptivePWM CLI Makefile"
	@echo "========================"
	@echo "Targets:"
	@echo "  all      - Build the CLI tool"
	@echo "  clean    - Remove build files"
	@echo "  install  - Install to /usr/local/bin"
	@echo "  config   - Create configuration directory"
	@echo "  help     - Show this help message"

.PHONY: all directories clean install config help