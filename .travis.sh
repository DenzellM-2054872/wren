#!/bin/bash
set -e

# This build script only builds mac or linux right now, for CI.
FODI_WD="projects/make"
if [ -n "$FODI_TARGET_MAC" ]; then
  FODI_WD="projects/make.mac"
fi

FODI_PY=${FODI_PY_BINARY:-python3}

echo "using working directory '$FODI_WD' ..."
echo "using python binary '$FODI_PY' ..."

make -C $FODI_WD config=debug_64bit-no-nan-tagging
$FODI_PY ./util/test.py --suffix=_d

make -C $FODI_WD config=debug_64bit
$FODI_PY ./util/test.py --suffix=_d

make -C $FODI_WD config=release_64bit-no-nan-tagging
$FODI_PY ./util/test.py

make -C $FODI_WD config=release_64bit
$FODI_PY ./util/test.py
