#!/usr/bin/env bash
set -e

BUILD_DIR="build"

# Configure (safe rebuild)
if [ ! -d "$BUILD_DIR" ]; then
  echo "📁 Creating build directory..."
  mkdir -p "$BUILD_DIR"
fi

echo "⚙️  Running CMake configure..."
cmake -S . -B "$BUILD_DIR"

echo "🔨 Building Verilog simulation (sim_compile)..."
cmake --build "$BUILD_DIR" --target sim_compile -j"$(nproc)"

echo "✅ Compilation complete."
