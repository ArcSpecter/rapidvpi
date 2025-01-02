#!/bin/bash

# Exit script on any error
set -e

# Step 1: Build vip_template
echo "Building vip_template..."
cd ./vip_template
mkdir -p cmake-build-debug && cd cmake-build-debug
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --target vip_template -j$(nproc)

# Step 2: Prepare and run rtl_template
echo "Building and running rtl_template..."
cd ../../rtl_template
mkdir build
cd ./build
cmake .. && cd ..
cmake --build ./build/ --target sim_compile
cmake --build ./build/ --target sim_run

echo "All tasks completed successfully!"
