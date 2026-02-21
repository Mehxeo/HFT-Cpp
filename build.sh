#!/bin/bash

# Build script for High-Performance Order Book
# Supports multiple build configurations

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="build"
EXECUTABLE="orderbook_demo"

echo "=========================================="
echo "High-Performance Order Book - Build Script"
echo "=========================================="

# Detect compiler
if command -v clang++ &> /dev/null; then
    CXX="clang++"
    echo -e "${GREEN}Using compiler: clang++${NC}"
elif command -v g++ &> /dev/null; then
    CXX="g++"
    echo -e "${GREEN}Using compiler: g++${NC}"
else
    echo -e "${RED}Error: No C++ compiler found!${NC}"
    echo "Please install clang++ or g++"
    exit 1
fi

# Check C++ version support
$CXX --version

# Parse command line arguments
BUILD_TYPE="release"
if [ "$1" == "debug" ]; then
    BUILD_TYPE="debug"
    echo -e "${YELLOW}Building in DEBUG mode${NC}"
elif [ "$1" == "clean" ]; then
    echo "Cleaning build artifacts..."
    rm -f $EXECUTABLE
    rm -rf $BUILD_DIR
    echo -e "${GREEN}Clean complete!${NC}"
    exit 0
else
    echo -e "${GREEN}Building in RELEASE mode (optimized)${NC}"
fi

# Build flags
CXX_FLAGS="-std=c++17 -Wall -Wextra -pthread"

if [ "$BUILD_TYPE" == "release" ]; then
    CXX_FLAGS="$CXX_FLAGS -O3 -march=native -mtune=native -DNDEBUG"
    echo "Optimization: -O3 -march=native"
else
    CXX_FLAGS="$CXX_FLAGS -O0 -g"
    echo "Optimization: -O0 -g (debug)"
fi

# Compile
echo ""
echo "Compiling..."
echo "Command: $CXX $CXX_FLAGS main.cpp -o $EXECUTABLE"
echo ""

$CXX $CXX_FLAGS main.cpp -o $EXECUTABLE

if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}✓ Build successful!${NC}"
    echo ""
    echo "Executable: ./$EXECUTABLE"
    
    # Show file size
    SIZE=$(ls -lh $EXECUTABLE | awk '{print $5}')
    echo "Size: $SIZE"
    echo ""
    echo "To run: ./$EXECUTABLE"
    echo "=========================================="
else
    echo -e "${RED}✗ Build failed!${NC}"
    exit 1
fi
