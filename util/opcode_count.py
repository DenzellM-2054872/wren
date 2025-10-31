#!/usr/bin/env python3
"""
Run a Wren program with an opcode-tracing build and count executed opcodes.

Usage:
  python3 util/opcode_count.py path/to/program.wren [--binary ./bin/wren_test_d] [--opcodes src/vm/wren_register_opcodes.h] [--out output.txt]

Assumptions:
  - The provided binary prints each executed opcode name in its stdout/stderr
    (e.g., via WREN_DEBUG_TRACE_INSTRUCTIONS or equivalent).
  - The header src/vm/wren_register_opcodes.h lists available opcodes using
    the X-macro pattern: REGOPCODE(NAME, ...)
"""

import argparse
import os
import re
import subprocess
import sys
from collections import Counter


def load_opcode_names(header_path: str) -> list[str]:
    pattern = re.compile(r"^\s*REGOPCODE\(\s*([A-Za-z0-9_]+)\s*,", re.MULTILINE)
    try:
        with open(header_path, "r", encoding="utf-8") as f:
            text = f.read()
    except FileNotFoundError:
        raise SystemExit(f"Opcode header not found: {header_path}")

    names = pattern.findall(text)
    if not names:
        raise SystemExit(f"No opcodes found in header: {header_path}")
    return names


def build_token_map(names: list[str]) -> tuple[re.Pattern, dict[str, str]]:
    """Create a regex to match any opcode token and a map token->base name.

    We accept these token variants and normalize them to the base NAME:
      - NAME (e.g., LOADK)
      - OP_NAME
      - op_NAME
    """
    tokens: list[str] = []
    base_for: dict[str, str] = {}
    for n in names:
        for t in (n, f"OP_{n}", f"op_{n}"):
            tokens.append(re.escape(t))
            base_for[t] = n
    # Use word boundaries to avoid partial matches.
    regex = re.compile(r"\\b(" + "|".join(tokens) + r")\\b")
    return regex, base_for


def run_and_count_stream(binary: str, wren_file: str, matcher: re.Pattern, token_to_base: dict[str, str], all_names: list[str] | None = None) -> Counter:
    """Run the binary and count opcodes by streaming the output line-by-line.

    Tries an exact token regex first (NAME, OP_NAME, op_NAME). If that fails
    for a line, falls back to case-insensitive tokenization against the known
    opcode names from the header.
    """
    if not os.path.isfile(binary):
        raise SystemExit(f"Binary not found: {binary}")
    if not os.path.isfile(wren_file):
        raise SystemExit(f"Wren file not found: {wren_file}")

    counts: Counter = Counter()
    names_set = set(n.upper() for n in (all_names or []))
    
    try:
        proc = subprocess.Popen(
            [binary, wren_file],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,  # Line buffered
        )
        
        # Process output line by line to avoid memory buildup
        for line in proc.stdout:
            matched = False
            for m in matcher.finditer(line):
                token = m.group(1)
                base = token_to_base.get(token, token)
                counts[base] += 1
                matched = True
            if matched:
                continue

            # Fallback: scan alpha/underscore tokens and match case-insensitively
            if names_set:
                for tok in re.findall(r"[A-Za-z_]+", line):
                    up = tok.upper()
                    if up in names_set:
                        counts[up] += 1
        
        proc.wait()
        
    except Exception as e:
        raise SystemExit(f"Failed to execute {binary}: {e}")

    return counts


def write_report(path: str, counts: Counter, all_names: list[str]) -> None:
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    total = sum(counts.values())

    # Sort: by count desc, then name asc
    items = sorted(counts.items(), key=lambda kv: (-kv[1], kv[0]))

    with open(path, "w", encoding="utf-8") as f:
        f.write(f"Total opcodes observed: {total}\n")
        f.write("Count\tOpcode\n")
        for name, cnt in items:
            if cnt <= 0:
                continue
            f.write(f"{cnt}\t{name}\n")

        # Optionally include missing opcodes with 0 at the end for completeness
        missing = [n for n in all_names if counts.get(n, 0) == 0]
        if missing:
            f.write("\n# Opcodes not observed (0 count):\n")
            f.write(", ".join(sorted(missing)))
            f.write("\n")


def main():
    parser = argparse.ArgumentParser(description="Run a Wren program and count executed opcodes from debug output.")
    parser.add_argument("wren_file", help="Path to the .wren source file to run")
    parser.add_argument("--binary", default="./bin/wren_test", help="Path to the Wren executable (default: ./bin/wren_test)")
    parser.add_argument("--opcodes", default="src/vm/wren_register_opcodes.h", help="Path to opcode header (default: src/vm/wren_register_opcodes.h)")
    parser.add_argument("--out", default=None, help="Output report file (default: ./data/opcode_counts/<wren_basename>_opcode_counts.txt)")

    args = parser.parse_args()

    names = load_opcode_names(args.opcodes)
    matcher, token_to_base = build_token_map(names)

    counts = run_and_count_stream(args.binary, args.wren_file, matcher, token_to_base, names)

    base = os.path.splitext(os.path.basename(args.wren_file))[0]
    out_path = args.out or os.path.join("data/opcode_counts", f"{base}_opcode_counts.txt")

    write_report(out_path, counts, names)

    print(f"Wrote opcode count report: {out_path}")


if __name__ == "__main__":
    main()
