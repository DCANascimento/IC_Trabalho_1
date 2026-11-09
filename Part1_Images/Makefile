# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude

# Directories
SRC_DIR = src
INC_DIR = include
PROG_DIR = programs
BUILD_DIR = build
BIN_DIR = bin

# Target executable name
TARGET = $(BIN_DIR)/img_hist

# Object files
OBJS = $(BUILD_DIR)/read_images.o \
       $(BUILD_DIR)/histograms.o \
       $(BUILD_DIR)/img_hist.o

# Default target
all: directories $(TARGET)

# Create build, bin, and Histograms directories if they don't exist
directories:
	mkdir -p $(BUILD_DIR) $(BIN_DIR) Histograms

# Link the executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Compile read_images.cpp
$(BUILD_DIR)/read_images.o: $(SRC_DIR)/read_images.cpp $(INC_DIR)/read_images.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile histograms.cpp
$(BUILD_DIR)/histograms.o: $(SRC_DIR)/histograms.cpp $(INC_DIR)/histograms.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile img_hist.cpp (main)
$(BUILD_DIR)/img_hist.o: $(PROG_DIR)/img_hist.cpp $(INC_DIR)/read_images.h $(INC_DIR)/histograms.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Phony targets
.PHONY: all directories clean