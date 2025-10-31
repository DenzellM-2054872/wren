#!/usr/bin/env bash
# profile_benchmarks.sh
# Runs all benchmark files with gprof-instrumented wren_test_d and saves profiles to separate files.
# Runs each benchmark multiple times and aggregates the profiling data for accuracy.

set -e

# Configuration
WREN_TEST="./bin/wren_test_d"
BENCHMARK_DIR="test/benchmark"
OUTPUT_DIR="./data/profile_output"
GPROF_BIN="gprof"
RUNS_PER_BENCHMARK=10  # Number of times to run each benchmark

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
echo "Running each benchmark $RUNS_PER_BENCHMARK times for accuracy"
echo "Output directory: $OUTPUT_DIR"
echo ""

# Run each benchmark and generate a profile
for bench in $BENCHMARKS; do
  # Extract benchmark name for output file (e.g., test/benchmark/fib.wren -> fib)
  bench_name=$(basename "$bench" .wren)
  
  echo "Running: $bench_name ($RUNS_PER_BENCHMARK runs)"
  
  # Remove old gmon.out files
  rm -f gmon.out gmon.out.* gmon.sum
  
  FAILED=0
  
  # Run the benchmark multiple times to accumulate profiling data
  for ((i=1; i<=RUNS_PER_BENCHMARK; i++)); do
    echo -n "  Run $i/$RUNS_PER_BENCHMARK..."
    
    # Run the benchmark (gmon.out will be created in current directory)
    "$WREN_TEST" "$bench" > /dev/null 2>&1 || {
      echo " FAILED"
      FAILED=1
      break
    }
    
    # Check if gmon.out was created
    GMON_FILE=$(ls gmon.out* 2>/dev/null | head -n 1)
    if [ -z "$GMON_FILE" ]; then
      echo " no gmon.out"
      FAILED=1
      break
    fi
    
    # Accumulate the profiling data using gprof -s (sum)
    if [ $i -eq 1 ]; then
      # First run: just rename to gmon.sum
      mv "$GMON_FILE" gmon.sum
    else
      # Subsequent runs: accumulate into gmon.sum
      "$GPROF_BIN" -s "$WREN_TEST" "$GMON_FILE" gmon.sum > /dev/null 2>&1
      rm -f "$GMON_FILE"
    fi
    
    echo " done"
  done
  
  if [ $FAILED -eq 1 ]; then
    echo "  Warning: $bench_name failed, skipping profile"
    rm -f gmon.out gmon.out.* gmon.sum
    continue
  fi
  
  # Generate the aggregated profile report
  OUTPUT_FILE="$OUTPUT_DIR/${bench_name}_gprof.txt"
  "$GPROF_BIN" "$WREN_TEST" gmon.sum > "$OUTPUT_FILE" 2>/dev/null
  
  echo "  -> $OUTPUT_FILE"
  
  # Clean up gmon files
  rm -f gmon.sum gmon.out gmon.out.*
done

echo ""
echo "Done! Aggregated profile reports saved to $OUTPUT_DIR/"

