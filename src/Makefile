# Makefile for Simple VCS

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = vcs
SRC = main.c

# Default target
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

# Clean build files
clean:
	rm -f $(TARGET)

# Run the program
run:
	./$(TARGET)
