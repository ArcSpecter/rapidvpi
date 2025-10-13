#!/usr/bin/env bash
set -e

BUILD_DIR="build"

if [ ! -d "$BUILD_DIR" ]; then
  echo "❌ Build directory not found. Run ./compile.sh first."
  exit 1
fi

echo "🚀 Running simulation..."

# Record start time (in seconds with decimals)
start_time=$(date +%s.%N)

# Run the simulation target
cmake --build "$BUILD_DIR" --target sim_run

# Record end time
end_time=$(date +%s.%N)

# Calculate elapsed time (in seconds, possibly fractional)
elapsed=$(echo "$end_time - $start_time" | bc)

# Format nicely
if (( $(echo "$elapsed < 1" | bc -l) )); then
  printf "⏱️  Simulation finished in %.3f seconds\n" "$elapsed"
elif (( $(echo "$elapsed < 60" | bc -l) )); then
  printf "⏱️  Simulation finished in %.2f seconds\n" "$elapsed"
elif (( $(echo "$elapsed < 3600" | bc -l) )); then
  minutes=$(echo "$elapsed / 60" | bc)
  seconds=$(echo "$elapsed - ($minutes * 60)" | bc)
  printf "⏱️  Simulation finished in %d minutes %.2f seconds\n" "$minutes" "$seconds"
else
  hours=$(echo "$elapsed / 3600" | bc)
  minutes=$(echo "($elapsed - $hours * 3600) / 60" | bc)
  seconds=$(echo "$elapsed - ($hours * 3600 + $minutes * 60)" | bc)
  printf "⏱️  Simulation finished in %d hours %d minutes %.2f seconds\n" "$hours" "$minutes" "$seconds"
fi
