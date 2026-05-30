#!/usr/bin/env python3

import sys
from os.path import basename, dirname, join, realpath, isfile
from glob import iglob
import re

INCLUDE_PATTERN = re.compile(r'^\s*#include "([\w.]+)"')
GUARD_PATTERN = re.compile(r'^#ifndef fodi(_\w+)?_h$')
FODI_DIR = dirname(dirname(realpath(__file__)))

seen_files = set()
out = sys.stdout

# Find a file in the different folders of the src dir.
def find_file(filename):
  names = [
    join(FODI_DIR, 'src', 'include', filename),
    join(FODI_DIR, 'src', 'vm', filename),
    join(FODI_DIR, 'src', 'optional', filename),
  ]
  for f in names:
    if isfile(f):
      return f
  raise Exception('File "{0}" not found!'.format(filename))

# Prints a plain text file, adding comment markers.
def add_comment_file(filename):
  with open(filename, 'r') as f:
    for line in f:
      out.write('// ')
      out.write(line)

# Prints the given C source file, recursively resolving local #includes.
def add_file(filename):
  bname = basename(filename)
  # Only include each file at most once.
  if bname in seen_files:
    return
  once = False

  out.write('// Begin file "{0}"\n'.format(bname))
  with open(filename, 'r') as f:
    for line in f:
      m = INCLUDE_PATTERN.match(line)
      if m:
        add_file(find_file(m.group(1)))
      else:
        out.write(line)
      if GUARD_PATTERN.match(line):
        once = True
  out.write('// End file "{0}"\n'.format(bname))

  # Only skip header files which use #ifndef guards.
  # This is necessary because of the X Macro technique.
  if once:
    seen_files.add(bname)

# Print license on top.
add_comment_file(join(FODI_DIR, 'LICENSE'))
out.write('\n')

# Source files.
add_file(join(FODI_DIR, 'src', 'include', 'fodi.h'))

# Must be included here because of conditional compilation.
add_file(join(FODI_DIR, 'src', 'vm', 'fodi_debug.h'))

for f in iglob(join(FODI_DIR, 'src', 'vm', '*.c')):
  add_file(f)

for f in iglob(join(FODI_DIR, 'src', 'optional', '*.c')):
  add_file(f)
