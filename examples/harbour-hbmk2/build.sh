#!/usr/bin/env bash
# build.sh — POSIX counterpart of build.cmd. Drives hbmk2 against
# Harbour's rddads contrib and OpenADS' libace.so / libace.dylib.
#
# Usage:
#   ./build.sh                        # defaults to ../../build/default/src
#   ./build.sh /path/to/openads/build/src
#
# Prereqs: Harbour 3.2 with contrib/rddads built; HB_INSTALL exported
# (or hbmk2 on PATH).

set -eu
OPENADS_LIB="${1:-${PWD}/../../build/default/src}"
export OPENADS_LIB

if [[ ! -e "${OPENADS_LIB}/libace.so" && ! -e "${OPENADS_LIB}/libace.dylib" ]]; then
    echo "[hbmk2] WARNING: no libace.{so,dylib} under ${OPENADS_LIB}"
fi

echo "[hbmk2] building openads_demo (OPENADS_LIB=${OPENADS_LIB}) ..."
hbmk2 openads_demo.hbp

echo "[hbmk2] done. Run with OpenADS' libace on LD_LIBRARY_PATH:"
echo "  LD_LIBRARY_PATH=\"${OPENADS_LIB}:\$LD_LIBRARY_PATH\" ./openads_demo"
