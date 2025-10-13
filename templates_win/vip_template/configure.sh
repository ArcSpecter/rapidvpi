#!/usr/bin/env bash
set -e

# Infer project name from current directory
PROJECT_NAME=$(basename "$(pwd)")
BUILD_DIR="cmake-build-release"

# Ensure VPI_INCLUDE_DIR is set
if [ -z "$VPI_INCLUDE_DIR" ]; then
  echo "❌ Error: Please export VPI_INCLUDE_DIR before running this script."
  echo "   Example: export VPI_INCLUDE_DIR=/home/$(whoami)/tools/iverilog"
  exit 1
fi

# Remove old build dir if it exists
if [ -d "$BUILD_DIR" ]; then
  echo "🧹 Removing existing $BUILD_DIR..."
  rm -rf "$BUILD_DIR"
fi

# Recreate and configure
echo "📦 Configuring ${PROJECT_NAME} (Release, Ninja)..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH="/usr/local/lib/cmake/rapidvpi" \
      -Dvpi_include_dir="$VPI_INCLUDE_DIR" ..
