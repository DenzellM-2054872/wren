#!/bin/bash

# Script to run all benchmarks and dump bytecode to separate files
# Output files will be saved in bytecode_output/ directory

# Create output directory
OUTPUT_DIR="bytecode_output"
mkdir -p "$OUTPUT_DIR"

# Path to the wren test executable
WREN_EXEC="./bin/wren_test_d"

# Check if executable exists
if [ ! -f "$WREN_EXEC" ]; then
    echo "Error: $WREN_EXEC not found. Please build it first."
    exit 1
fi

# Find all benchmark files
BENCHMARK_DIR="test/benchmark"
BENCHMARKS=$(find "$BENCHMARK_DIR" -name "*.wren" -type f | sort)

echo "Dumping bytecode for all benchmarks..."
echo "Output directory: $OUTPUT_DIR"
echo ""

# Process each benchmark
for benchmark in $BENCHMARKS; do
    # Extract just the filename without path and extension
    filename=$(basename "$benchmark" .wren)
    
    # Output file path
    output_file="$OUTPUT_DIR/${filename}_bytecode.txt"
    
    echo "Processing: $benchmark -> $output_file"
    
    # Run the benchmark and capture output
    "$WREN_EXEC" "$benchmark" > "$output_file" 2>&1
    
    # Check if the command succeeded
    if [ $? -eq 0 ]; then
        echo "  ✓ Success ($(wc -l < "$output_file") lines)"
    else
        echo "  ✗ Failed (exit code: $?)"
    fi
done

echo ""
echo "Done! Bytecode files saved to $OUTPUT_DIR/"
echo "Total files: $(ls -1 "$OUTPUT_DIR" | wc -l)"
