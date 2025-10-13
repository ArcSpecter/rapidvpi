#!/usr/bin/env bash
set -e

BUILD_DIR="build"

if [ -d "$BUILD_DIR" ]; then
  echo "🧹 Removing old build directory..."
  rm -rf "$BUILD_DIR"
fi

echo "📁 Creating new build directory..."
mkdir -p "$BUILD_DIR"

echo "✅ Clean build folder ready."
