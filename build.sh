#!/bin/bash
# Build script for thermal imaging system
# Compiles all source files and links them into an executable

set -e  # Exit on any error

echo "======================================"
echo "Thermal Imaging System - Build Script"
echo "======================================"
echo

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
BIN_DIR="$BUILD_DIR/bin"

# Create build directory
mkdir -p "$BIN_DIR"

echo -e "${YELLOW}Setting up build directory...${NC}"
echo "Project: $PROJECT_DIR"
echo "Build:   $BUILD_DIR"
echo

# Check for C++ compiler
if ! command -v clang++ &> /dev/null; then
    echo -e "${RED}Error: clang++ not found. Please install LLVM/Clang.${NC}"
    exit 1
fi

COMPILER=$(which clang++)
echo -e "${GREEN}Using compiler: $COMPILER${NC}"
echo

# Compiler flags for optimization
CXXFLAGS="-std=c++17 -Wall -Wextra -O2 -ffunction-sections -fdata-sections"
LDFLAGS="-Wl,-dead_strip"

# Source files
SOURCES=(
    "MLX90640Driver.cpp"
    "MLX90640Upscaler.cpp"
    "TargetLockTracker.cpp"
    "ThermalUIRenderer.cpp"
    "target_tracking_example.cpp"
)

# Output executable
EXECUTABLE="$BIN_DIR/thermal_targeting_system"

# Compile and link
echo -e "${YELLOW}Compiling source files...${NC}"
$COMPILER $CXXFLAGS -o "$EXECUTABLE" "${SOURCES[@]}" $LDFLAGS

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Compilation successful!${NC}"
    echo
    echo -e "${GREEN}Executable created: $EXECUTABLE${NC}"
    echo
    echo "To run the thermal imaging system:"
    echo -e "${YELLOW}  $EXECUTABLE${NC}"
    echo
else
    echo -e "${RED}✗ Compilation failed!${NC}"
    exit 1
fi

# Display file statistics
echo -e "${YELLOW}Build Statistics:${NC}"
ls -lh "$EXECUTABLE"
echo

exit 0
