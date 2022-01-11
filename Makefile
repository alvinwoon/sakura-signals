CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -pedantic
LDFLAGS = -lm
TARGET = sakura_signals_demo
SOURCES = demo.c circular_buffer.c statistics.c correlation.c cointegration.c signals.c attention.c regime_detection.c dynamic_hedging.c transaction_costs.c risk_management.c simd_optimizations.c advanced_cointegration.c
OBJECTS = $(SOURCES:.c=.o)
HEADER = sakura_signals.h

# Default target
all: $(TARGET)

# Build the main executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

# Compile individual object files
%.o: %.c $(HEADER)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)

# Install (optional - copies to /usr/local/bin)
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

# Uninstall
uninstall:
	rm -f /usr/local/bin/$(TARGET)

# Run the demo
run: $(TARGET)
	./$(TARGET)

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# Build with address sanitizer (useful for debugging)
asan: CFLAGS += -fsanitize=address -g
asan: LDFLAGS += -fsanitize=address
asan: $(TARGET)

# Static analysis with clang
analyze:
	clang --analyze $(CFLAGS) $(SOURCES)

# Format code (requires clang-format)
format:
	clang-format -i *.c *.h

# Check for memory leaks (requires valgrind - install via homebrew)
memcheck: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET)

# Print help
help:
	@echo "Available targets:"
	@echo "  all      - Build the demo executable (default)"
	@echo "  clean    - Remove build artifacts"
	@echo "  run      - Build and run the demo"
	@echo "  debug    - Build with debug symbols"
	@echo "  asan     - Build with AddressSanitizer"
	@echo "  analyze  - Run static analysis"
	@echo "  format   - Format source code"
	@echo "  memcheck - Check for memory leaks (requires valgrind)"
	@echo "  install  - Install to /usr/local/bin"
	@echo "  help     - Show this help message"

.PHONY: all clean install uninstall run debug asan analyze format memcheck help