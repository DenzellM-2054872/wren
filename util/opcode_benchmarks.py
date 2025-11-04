#!/usr/bin/env python3
"""
Run all Wren benchmarks, capture opcode count output, and write results to ./data/opcode_counts.

Behavior:
- For each benchmark .wren file in test/benchmark, execute it with the wren_test binary.
- Each run prints two "OPCODE COUNTS" sections:
  - The first section is a baseline (always the same) — recorded once to baseline.txt.
  - The second section contains the benchmark's opcode counts — recorded per benchmark.
- Any opcodes not present in the parsed benchmark section are listed under a "NOT APPEARING" section.

Outputs:
- ./data/opcode_counts/baseline.txt (written once from the first benchmark run)
- ./data/opcode_counts/benchmarks/<benchmark_name>_opcode_counts.txt

Options:
--wren-bin PATH     Path to wren_test executable (defaults to ./bin/wren_test then ./bin/wren_test_d)
--bench-dir PATH    Benchmarks root (defaults to ./test/benchmark)
--out-dir PATH      Output root (defaults to ./data/opcode_counts)
--limit N           Limit number of benchmarks to run (for quick verification)
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Tuple


START_MARKER = " ========== OPCODE COUNTS ========== "
END_MARKER = " =================================== "
DISPATCH_PREFIX = "Dispatches: "
OPCODE_PREFIX = "Opcode: "


def repo_root_from_here() -> Path:
    return Path(__file__).resolve().parent.parent


def get_default_wren_bin(root: Path) -> Path:
    candidates = [root / "bin/wren_test", root / "bin/wren_test_d"]
    for c in candidates:
        if c.exists() and c.is_file():
            return c
    # Fall back to first candidate path for error messaging
    return candidates[0]


def load_canonical_opcodes(root: Path) -> List[str]:
    """Parse src/vm/wren_register_opcodes.h to get the authoritative opcode list in order."""
    header_path = root / "src/vm/wren_register_opcodes.h"
    text = header_path.read_text(encoding="utf-8")
    opcodes: List[str] = []
    for line in text.splitlines():
        m = re.search(r"REGOPCODE\((\w+),", line)
        if m:
            opcodes.append(m.group(1))
    if not opcodes:
        raise RuntimeError(f"No opcodes found in {header_path}")
    return opcodes


def parse_opcode_blocks(output: str) -> List[Tuple[Dict[str, int], int]]:
    """Parse all opcode count blocks from program output.

    Returns a list of tuples: (opcode_counts, dispatches)
    where opcode_counts is a dict mapping opcode name to count (ints), and dispatches is the parsed dispatch count (int or 0 if missing).
    """
    blocks: List[Tuple[Dict[str, int], int]] = []
    lines = output.splitlines()
    i = 0
    while i < len(lines):
        if lines[i].strip() == START_MARKER.strip():
            i += 1
            counts: Dict[str, int] = {}
            dispatches = 0
            # Read until end marker
            while i < len(lines) and lines[i].strip() != END_MARKER.strip():
                line = lines[i].strip()
                if line.startswith(DISPATCH_PREFIX):
                    try:
                        dispatches = int(line[len(DISPATCH_PREFIX):].strip())
                    except ValueError:
                        dispatches = 0
                elif line.startswith(OPCODE_PREFIX):
                    # Format: Opcode: NAME (COUNT)
                    m = re.match(r"Opcode:\s+(\w+)\s*\(([-\d]+)\)", line)
                    if m:
                        name = m.group(1)
                        try:
                            val = int(m.group(2))
                        except ValueError:
                            val = 0
                        if val > 0:
                            counts[name] = val
                i += 1
            # We are either at END_MARKER or EOF
            blocks.append((counts, dispatches))
        else:
            i += 1
    return blocks


def run_benchmark(wren_bin: Path, bench_file: Path) -> str:
    proc = subprocess.run(
        [str(wren_bin), str(bench_file)],
        text=True,
        capture_output=True,
        check=False,
    )
    # Combine stdout and stderr to be safe; most prints go to stdout.
    return (proc.stdout or "") + ("\n" + proc.stderr if proc.stderr else "")


def write_baseline(out_dir: Path, baseline_counts: Dict[str, int], dispatches: int, canonical: List[str]) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    baseline_path = out_dir / "baseline.txt"
    lines: List[str] = []
    lines.append("========== OPCODE COUNTS (BASELINE) ==========")
    lines.append(f"Dispatches: {dispatches}")
    for op in canonical:
        val = baseline_counts.get(op, 0)
        lines.append(f"Opcode: {op} ({val})")
    lines.append("=============================================")
    baseline_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_benchmark(out_dir: Path, bench_name: str, counts: Dict[str, int], dispatches: int, canonical: List[str]) -> Path:
    bench_dir = out_dir
    bench_dir.mkdir(parents=True, exist_ok=True)
    out_path = bench_dir / f"{bench_name}_opcode_counts.txt"

    seen = set(counts.keys())
    missing = [op for op in canonical if op not in seen]

    lines: List[str] = []
    lines.append("========== OPCODE COUNTS (BENCHMARK) ==========")
    lines.append(f"Benchmark: {bench_name}")
    lines.append(f"Dispatches: {dispatches}")
    for op in canonical:
        val = counts.get(op, 0)
        if op in seen:
            lines.append(f"Opcode: {op} ({val})")
    lines.append("========== NOT APPEARING ==========")
    if missing:
        for op in missing:
            lines.append(op)
    else:
        lines.append("(none)")
    lines.append("===================================")

    out_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return out_path


def find_benchmarks(bench_dir: Path) -> List[Path]:
    return sorted(bench_dir.rglob("*.wren"))


def main(argv: List[str]) -> int:
    root = repo_root_from_here()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wren-bin", dest="wren_bin", type=Path, default=None)
    parser.add_argument("--bench-dir", dest="bench_dir", type=Path, default=root / "test/benchmark")
    parser.add_argument("--out-dir", dest="out_dir", type=Path, default=root / "data/opcode_counts")
    parser.add_argument("--limit", dest="limit", type=int, default=None)
    args = parser.parse_args(argv)

    wren_bin = args.wren_bin or get_default_wren_bin(root)
    if not wren_bin.exists():
        print(f"Error: wren binary not found at {wren_bin}. Build it first.", file=sys.stderr)
        return 1

    canonical = load_canonical_opcodes(root)

    benches = find_benchmarks(args.bench_dir)
    if not benches:
        print(f"No benchmarks found in {args.bench_dir}", file=sys.stderr)
        return 1

    # Prepare output dir
    out_dir: Path = args.out_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    baseline_written = False
    limit = args.limit if args.limit and args.limit > 0 else None

    count_run = 0
    for bench_path in benches:
        if limit is not None and count_run >= limit:
            break

        bench_name = bench_path.stem
        print(f"Running {bench_name}...")
        output = run_benchmark(wren_bin, bench_path)
        blocks = parse_opcode_blocks(output)
        if not blocks:
            print(f"  Warning: No opcode blocks found for {bench_name}. Skipping.")
            continue

        # Heuristic: use first block as baseline, last block as benchmark
        baseline_counts, baseline_dispatches = blocks[0]
        bench_counts, bench_dispatches = blocks[-1]

        # Write baseline once
        if not baseline_written:
            write_baseline(out_dir, baseline_counts, baseline_dispatches, canonical)
            baseline_written = True

        out_path = write_benchmark(out_dir, bench_name, bench_counts, bench_dispatches, canonical)
        print(f"  -> {out_path.relative_to(root)}")
        count_run += 1

    print("Done.")
    print(f"Outputs in: {out_dir.relative_to(root)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
