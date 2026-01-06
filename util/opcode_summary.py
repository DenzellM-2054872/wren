#!/usr/bin/env python3
"""
Generate a summary of opcode counts from all benchmark files.
Outputs a tab-separated format that can be copied into Google Sheets.
"""

import os
import re
import sys
from pathlib import Path
from collections import defaultdict

# Get the data directory
SCRIPT_DIR = Path(__file__).parent
DATA_DIR = SCRIPT_DIR.parent / "data" / "opcode_counts"
OUTPUT_FILE = SCRIPT_DIR.parent / "data" / "opcode_summary.tsv"


def parse_opcode_file(filepath):
    """Parse an opcode count file and return a dictionary of opcode counts."""
    counts = {}
    benchmark_name = None
    
    with open(filepath, 'r') as f:
        content = f.read()
        
        # Try to extract benchmark name
        name_match = re.search(r'Benchmark: (\w+)', content)
        if name_match:
            benchmark_name = name_match.group(1)
        else:
            # Use filename as fallback
            benchmark_name = filepath.stem.replace('_opcode_counts', '')
        
        # Parse opcode lines - handle both formats
        # Format 1: "Opcode: NAME (count)"
        for match in re.finditer(r'Opcode: (\w+) \((\d+)\)', content):
            opcode = match.group(1)
            count = int(match.group(2))
            counts[opcode] = count
        
        # Format 2: "NAME: count [percentage%]"
        for match in re.finditer(r'^([A-Z][A-Z0-9]*): (\d+)', content, re.MULTILINE):
            opcode = match.group(1)
            count = int(match.group(2))
            # Skip lines that aren't opcodes (like "Dispatches", "Total", etc.)
            if opcode not in ['Dispatches', 'Total', 'Benchmark']:
                counts[opcode] = count
    
    return benchmark_name, counts


def get_all_opcodes(all_counts):
    """Get a sorted list of all unique opcodes across all benchmarks."""
    opcodes = set()
    for counts in all_counts.values():
        opcodes.update(counts.keys())
    return sorted(opcodes)


def main():
    # Dictionary to store counts for each benchmark
    all_counts = {}
    
    # Process all opcode count files
    if not DATA_DIR.exists():
        print(f"Error: Directory {DATA_DIR} does not exist")
        return
    
    for filepath in sorted(DATA_DIR.glob("*.txt")):
        if filepath.name != "baseline.txt":  # Skip baseline
            benchmark_name, counts = parse_opcode_file(filepath)
            all_counts[benchmark_name] = counts
    
    if not all_counts:
        print("No opcode count files found!")
        return
    
    # Get all unique opcodes
    all_opcodes = get_all_opcodes(all_counts)
    
    # Get sorted benchmark names
    benchmark_names = sorted(all_counts.keys())
    
    # Allow custom output file via command line argument
    output_path = OUTPUT_FILE
    if len(sys.argv) > 1:
        output_path = Path(sys.argv[1])
    
    # Write to file
    with open(output_path, 'w') as f:
        # Write header row
        f.write("Opcode\t" + "\t".join(benchmark_names) + "\n")
        
        # Write data rows
        for opcode in all_opcodes:
            row = [opcode]
            for benchmark in benchmark_names:
                count = all_counts[benchmark].get(opcode, 0)
                row.append(str(count))
            f.write("\t".join(row) + "\n")
        
        # Write totals row
        row = ["TOTAL"]
        for benchmark in benchmark_names:
            total = sum(all_counts[benchmark].values())
            row.append(str(total))
        f.write("\t".join(row) + "\n")
    
    print(f"Opcode summary written to: {output_path}")


if __name__ == "__main__":
    main()
