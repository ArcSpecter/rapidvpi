#!/usr/bin/env bash
set -e

echo "🔨 Building RapidVPI..."
cd cmake-build-release
cmake --build . --target rapidvpi.vpi -j"$(nproc)"
cd ..
