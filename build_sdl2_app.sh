#!/bin/bash

# Thermal Imaging System - SDL2 Desktop App Build Script

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_DIR="$SCRIPT_DIR/build"
APP_NAME="thermal_imaging_app"

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Thermal Imaging Desktop App - Build${NC}"
echo -e "${BLUE}========================================${NC}\n"

# Create build directory
mkdir -p "$BUILD_DIR/bin"

# Check for clang++
if ! command -v clang++ &> /dev/null; then
    echo -e "${RED}✗ clang++ not found${NC}"
    exit 1
fi

echo -e "${GREEN}✓${NC} Using clang++ at $(which clang++)"

# Check for SDL2
if ! pkg-config --exists sdl2; then
    echo -e "${RED}✗ SDL2 not found. Installing...${NC}"
    if command -v brew &> /dev/null; then
        brew install sdl2
        echo -e "${GREEN}✓${NC} SDL2 installed via Homebrew"
    else
        echo -e "${RED}Please install SDL2:${NC}"
        echo "  macOS: brew install sdl2"
        echo "  Ubuntu: sudo apt-get install libsdl2-dev"
        exit 1
    fi
fi

SDL2_CFLAGS=$(pkg-config --cflags sdl2)
SDL2_LIBS=$(pkg-config --libs sdl2)

echo -e "${GREEN}✓${NC} SDL2 found"

# Compiler flags
CFLAGS="-std=c++17 -Wall -Wextra -O2 -ffunction-sections -fdata-sections"
LFLAGS="-Wl,-dead_strip"

# Source files
SOURCES=(
    "MLX90640Driver.cpp"
    "MLX90640Upscaler.cpp"
    "TargetLockTracker.cpp"
    "ThermalUIRenderer.cpp"
    "thermal_app_sdl2.cpp"
)

echo -e "\n${BLUE}Compiling sources...${NC}"

for source in "${SOURCES[@]}"; do
    echo -e "  ${source}..."
done

# Compile
OUTPUT_FILE="$BUILD_DIR/bin/$APP_NAME"

clang++ $CFLAGS $SDL2_CFLAGS \
    "${SOURCES[@]}" \
    $SDL2_LIBS \
    $LFLAGS \
    -o "$OUTPUT_FILE"

if [ $? -eq 0 ]; then
    APP_SIZE=$(ls -lh "$OUTPUT_FILE" | awk '{print $5}')
    echo -e "\n${GREEN}✓ Compilation successful!${NC}"
    echo -e "\nExecutable created: ${GREEN}$OUTPUT_FILE${NC}"
    echo -e "Size: ${GREEN}${APP_SIZE}${NC}\n"
    
    # Create launch script for convenience
    LAUNCH_SCRIPT="$SCRIPT_DIR/run_thermal_app.sh"
    cat > "$LAUNCH_SCRIPT" << 'EOF'
#!/bin/bash
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
"$SCRIPT_DIR/build/bin/thermal_imaging_app"
EOF
    chmod +x "$LAUNCH_SCRIPT"
    echo -e "Launch script: ${GREEN}$LAUNCH_SCRIPT${NC}"
    
    echo -e "\n${BLUE}To run the app:${NC}"
    echo -e "  ${GREEN}$LAUNCH_SCRIPT${NC}"
    echo -e "  or"
    echo -e "  ${GREEN}$OUTPUT_FILE${NC}"
else
    echo -e "\n${RED}✗ Compilation failed!${NC}"
    exit 1
fi
