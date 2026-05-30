#!/usr/bin/env python3
"""
Run a Fodi file with fodi_test and count occurrences of class operation patterns.

Pattern format: [ClassName: number]

Usage:
    python3 count_class_operations.py <fodi_file>
    python3 count_class_operations.py --fodi-bin <path> <fodi_file>
"""

import argparse
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path


def repo_root_from_here() -> Path:
    return Path(__file__).resolve().parent.parent


def get_default_fodi_bin(root: Path) -> Path:
    candidates = [root / "bin/fodi_test", root / "bin/fodi_test_d"]
    for c in candidates:
        if c.exists() and c.is_file():
            return c
    return candidates[0]


def run_fodi_file(fodi_bin: Path, fodi_file: Path) -> str:
    """Run the fodi file and capture all output."""
    proc = subprocess.run(
        [str(fodi_bin), str(fodi_file)],
        text=True,
        capture_output=True,
        check=False,
    )
    # Combine stdout and stderr
    return (proc.stdout or "") + ("\n" + proc.stderr if proc.stderr else "")


def count_class_operations(output: str) -> dict:
    """
    Parse output for patterns like [ClassName: number] and count occurrences.
    
    Returns a dict mapping (class_name, number) tuples to their counts.
    """
    # Pattern to match [ClassName: number]
    pattern = r'\[([^:\]]+):\s*(\d+)\]'
    
    counts = defaultdict(int)
    
    for match in re.finditer(pattern, output):
        class_name = match.group(1).strip()
        number = match.group(2).strip()
        key = (class_name, number)
        counts[key] += 1
    
    return counts


def main(argv: list) -> int:
    root = repo_root_from_here()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fodi_file", type=Path, help="Path to the .wren file to run")
    parser.add_argument("--fodi-bin", dest="fodi_bin", type=Path, default=None,
                        help="Path to fodi_test executable")
    args = parser.parse_args(argv)

    fodi_bin = args.wren_bin or get_default_fodi_bin(root)
    if not fodi_bin.exists():
        print(f"Error: fodi binary not found at {fodi_bin}. Build it first.", file=sys.stderr)
        return 1

    fodi_file = args.wren_file
    if not fodi_file.exists():
        print(f"Error: Fodi file not found at {fodi_file}", file=sys.stderr)
        return 1

    print(f"Running {fodi_file} with {fodi_bin.name}...")
    output = run_fodi_file(fodi_bin, fodi_file)
    
    counts = count_class_operations(output)
    if not counts:
        print("\nNo class operation patterns found in output.")
        return 0
    
    print("\n" + "=" * 60)
    print("CLASS OPERATION COUNTS")
    print("=" * 60)
    
    # Sort by count (descending), then by class name, then by number
    sorted_counts = sorted(counts.items(), key=lambda x: (-x[1], x[0][0], int(x[0][1])))
    
    print(f"{'Class Name':<30} {'Number':<10} {'Count':<10}")
    print("-" * 60)
    
    for (class_name, number), count in sorted_counts:
        print(f"{class_name:<30} {number:<10} {count:<10}")
    
    print("-" * 60)
    print(f"Total unique patterns: {len(counts)}")
    print(f"Total occurrences: {sum(counts.values())}")
    
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
