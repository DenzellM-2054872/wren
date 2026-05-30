#!/usr/bin/env python
# coding: utf-8

import argparse
import glob
import os.path
import re

# The source for the Fodi modules that are built into the VM or CLI are turned
# include C string literals. This way they can be compiled directly into the
# code so that file IO is not needed to find and read them.
#
# These string literals are stored in files with a ".wren.inc" extension and
# #included directly by other source files. This generates a ".wren.inc" file
# given a ".wren" module.

PREAMBLE = """// Generated automatically from {0}. Do not edit.
static const char* {1}ModuleSource =
{2};
"""

def fodi_to_c_string(input_path, fodi_source_lines, module):
  fodi_source = ""
  for line in fodi_source_lines:
    line = line.replace("\\", "\\\\")
    line = line.replace('"', "\\\"")
    line = line.replace("\n", "\\n\"")
    if fodi_source: fodi_source += "\n"
    fodi_source += '"' + line

  return PREAMBLE.format(input_path, module, fodi_source)


def main():
  parser = argparse.ArgumentParser(
      description="Convert a Fodi library to a C string literal.")
  parser.add_argument("output", help="The output file to write")
  parser.add_argument("input", help="The source .wren file")

  args = parser.parse_args()

  with open(args.input, "r") as f:
    fodi_source_lines = f.readlines()

  module = os.path.splitext(os.path.basename(args.input))[0]
  module = module.replace("opt_", "")
  module = module.replace("fodi_", "")

  c_source = fodi_to_c_string(args.input, fodi_source_lines, module)

  with open(args.output, "w") as f:
    f.write(c_source)


main()
