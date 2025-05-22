# Makefile for VCS (Version Control System)

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
LDFLAGS = 

# Program name and source files
PROGRAM = vcs
SOURCE = vcs.c
OBJECT = $(SOURCE:.c=.o)

# Installation directories
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

# Default target
all: $(PROGRAM)

# Compile the program
$(PROGRAM): $(OBJECT)
	$(CC) $(LDFLAGS) -o $(PROGRAM) $(OBJECT)
	@echo "VCS compiled successfully!"

# Compile object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Install the program system-wide
install: $(PROGRAM)
	@echo "Installing VCS to $(BINDIR)..."
	@if [ ! -d "$(BINDIR)" ]; then \
		echo "Creating directory $(BINDIR)..."; \
		sudo mkdir -p $(BINDIR); \
	fi
	sudo cp $(PROGRAM) $(BINDIR)/$(PROGRAM)
	sudo chmod 755 $(BINDIR)/$(PROGRAM)
	@echo "VCS installed successfully!"
	@echo "You can now use 'vcs' command from anywhere."

# Uninstall the program
uninstall:
	@echo "Removing VCS from $(BINDIR)..."
	@if [ -f "$(BINDIR)/$(PROGRAM)" ]; then \
		sudo rm -f $(BINDIR)/$(PROGRAM); \
		echo "VCS uninstalled successfully!"; \
	else \
		echo "VCS is not installed in $(BINDIR)"; \
	fi

# Clean compiled files
clean:
	@echo "Cleaning up..."
	rm -f $(OBJECT) $(PROGRAM)
	@echo "Clean completed!"

# Check if VCS is installed
check-install:
	@if [ -f "$(BINDIR)/$(PROGRAM)" ]; then \
		echo "VCS is installed at $(BINDIR)/$(PROGRAM)"; \
		$(BINDIR)/$(PROGRAM) 2>/dev/null || echo "Version: Custom VCS v1.0"; \
	else \
		echo "VCS is not installed"; \
	fi

# Run tests (basic functionality test)
test: $(PROGRAM)
	@echo "Running basic VCS tests..."
	@./$(PROGRAM) > /dev/null 2>&1 && echo "✓ VCS executable works" || echo "✗ VCS executable failed"
	@echo "Test completed!"

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: clean $(PROGRAM)
	@echo "Debug build completed!"

# Release build  
release: CFLAGS += -DNDEBUG
release: clean $(PROGRAM)
	@echo "Release build completed!"

# Help target
help:
	@echo "VCS Makefile Commands:"
	@echo "  make          - Compile the vcs program"
	@echo "  make install  - Copy it to /usr/local/bin (system-wide)"
	@echo "  make uninstall- Remove it from /usr/local/bin"
	@echo "  make clean    - Remove the compiled binary (vcs)"
	@echo "  make test     - Run basic functionality tests"
	@echo "  make debug    - Compile with debug symbols"
	@echo "  make release  - Compile optimized release version"
	@echo "  make check-install - Check if VCS is installed"
	@echo "  make help     - Show this help message"

# Declare phony targets
.PHONY: all install uninstall clean check-install test debug release help

# Default goal
.DEFAULT_GOAL := all