#!/usr/bin/env bash
# profile_benchmarks.sh
# Runs all benchmark files with gprof-instrumented wren_test_d and saves profiles to separate files.

set -e

# Configuration
WREN_TEST="./bin/wren_test_d"
BENCHMARK_DIR="test/benchmark"
OUTPUT_DIR="./profile_output"
GPROF_BIN="gprof"

# Check that the binary exists and is instrumented
if [ ! -f "$WREN_TEST" ]; then
  echo "Error: $WREN_TEST not found. Build with 'profile: gprof report' task first."
  exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Find all .wren files in the benchmark directory
BENCHMARKS=$(find "$BENCHMARK_DIR" -name "*.wren" | sort)

if [ -z "$BENCHMARKS" ]; then
  echo "No benchmark files found in $BENCHMARK_DIR"
  exit 1
fi

echo "Found $(echo "$BENCHMARKS" | wc -l) benchmark files"
echo "Output directory: $OUTPUT_DIR"
echo ""

# Run each benchmark and generate a profile
for bench in $BENCHMARKS; do
  # Extract benchmark name for output file (e.g., test/benchmark/fib.wren -> fib)
  bench_name=$(basename "$bench" .wren)
  
  echo "Running: $bench_name"
  
  # Remove old gmon.out files
  rm -f gmon.out gmon.out.*
  
  # Run the benchmark (gmon.out will be created in current directory)
  "$WREN_TEST" "$bench" > /dev/null 2>&1 || {
    echo "  Warning: $bench_name failed or crashed, skipping profile"
    continue
  }
  
  # Check if gmon.out was created
  GMON_FILE=$(ls gmon.out* 2>/dev/null | head -n 1)
  if [ -z "$GMON_FILE" ]; then
    echo "  Warning: no gmon.out generated for $bench_name (binary may not be instrumented with -pg)"
    continue
  fi
  
  # Generate the profile report
  OUTPUT_FILE="$OUTPUT_DIR/${bench_name}_gprof.txt"
  "$GPROF_BIN" "$WREN_TEST" "$GMON_FILE" > "$OUTPUT_FILE" 2>/dev/null
  
  echo "  -> $OUTPUT_FILE"
  
  # Clean up gmon files
  rm -f "$GMON_FILE"
done

echo ""
echo "Done! Profile reports saved to $OUTPUT_DIR/"

