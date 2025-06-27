# Makefile for VCS (Version Control System)

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
LDFLAGS = 

# Program name and source files
PROGRAM = vcs
SOURCE = vcs.c
OBJECT = $(SOURCE:.c=.o)

# Installation directories (for macOS/Linux)
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
		$(BINDIR)/$(PROGRAM) --version 2>/dev/null || echo "Version: Custom VCS v1.0"; \
	else \
		echo "VCS is not installed"; \
	fi

# Run tests (basic functionality test)
test: $(PROGRAM)
	@echo "Running basic VCS tests..."
	@./$(PROGRAM) > /dev/null 2>&1 && echo "✓ VCS executable works" || echo "✗ VCS executable failed"
	@mkdir -p test_repo && cd test_repo && \
	 (../$(PROGRAM) init && \
	  touch test_file && \
	  ../$(PROGRAM) add test_file && \
	  ../$(PROGRAM) commit "Test commit" && \
	  ../$(PROGRAM) status) 2>/dev/null && echo "✓ Basic functionality works" || echo "✗ Basic functionality failed"
	@rm -rf test_repo
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
	@echo "  make install  - Install system-wide to $(BINDIR)"
	@echo "  make uninstall- Remove from system"
	@echo "  make clean    - Remove compiled files"
	@echo "  make test     - Run basic functionality tests"
	@echo "  make debug    - Build with debug symbols"
	@echo "  make release  - Build optimized release"
	@echo "  make check-install - Check if installed"
	@echo "  make help     - Show this help"

# Declare phony targets
.PHONY: all install uninstall clean check-install test debug release help

# Default goal
.DEFAULT_GOAL := all