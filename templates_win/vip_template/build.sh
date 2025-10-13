#!/usr/bin/env bash
set -e

PROJECT_NAME=$(basename "$(pwd)")

echo "🔨 Building ${PROJECT_NAME}..."
cd cmake-build-release
cmake --build . --target "${PROJECT_NAME}" -j"$(nproc)"
cd ..
