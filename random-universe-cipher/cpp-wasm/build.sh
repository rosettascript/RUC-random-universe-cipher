#!/bin/bash

# Build script for C++ WASM using Emscripten

set -e

# Try to source emsdk if not in PATH
if ! command -v emcc &> /dev/null; then
    if [ -f "$HOME/emsdk/emsdk_env.sh" ]; then
        echo "📦 Activating Emscripten SDK..."
        source "$HOME/emsdk/emsdk_env.sh"
    fi
fi

# Check if Emscripten is installed
if ! command -v emcc &> /dev/null; then
    echo "Error: Emscripten not found. Please install Emscripten first."
    echo "Run: bash setup-emscripten.sh"
    exit 1
fi

# Create build directory
mkdir -p build
cd build

# Configure with Emscripten
emcmake cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
emmake make -j$(nproc)

# Copy output to pkg directory
mkdir -p ../pkg
cp ruc_wasm.js ../pkg/
cp ruc_wasm.wasm ../pkg/

echo "✅ C++ WASM build complete!"
echo "Output files:"
echo "  - pkg/ruc_wasm.js"
echo "  - pkg/ruc_wasm.wasm"

