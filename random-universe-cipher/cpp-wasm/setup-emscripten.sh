#!/bin/bash

# Setup script to install Emscripten SDK

set -e

EMSDK_DIR="$HOME/emsdk"

echo "🔧 Setting up Emscripten SDK..."

# Check if already installed
if [ -d "$EMSDK_DIR" ] && [ -f "$EMSDK_DIR/emsdk_env.sh" ]; then
    echo "✅ Emscripten SDK found at $EMSDK_DIR"
    source "$EMSDK_DIR/emsdk_env.sh"
    if command -v emcc &> /dev/null; then
        echo "✅ Emscripten is already set up and working!"
        emcc --version | head -1
        exit 0
    fi
fi

# Install emsdk
if [ ! -d "$EMSDK_DIR" ]; then
    echo "📥 Cloning Emscripten SDK..."
    git clone https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
fi

cd "$EMSDK_DIR"

echo "📦 Installing Emscripten (this may take a few minutes)..."
./emsdk install latest

echo "🔗 Activating Emscripten..."
./emsdk activate latest

echo "✅ Emscripten SDK installed!"
echo ""
echo "To use it in this shell, run:"
echo "  source $EMSDK_DIR/emsdk_env.sh"
echo ""
echo "Or add to your ~/.bashrc:"
echo "  source $EMSDK_DIR/emsdk_env.sh"

# Activate for current shell
source "$EMSDK_DIR/emsdk_env.sh"

echo ""
echo "✅ Emscripten is ready!"
emcc --version | head -1

