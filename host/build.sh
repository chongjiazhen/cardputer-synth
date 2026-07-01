#!/usr/bin/env bash
# Build the desktop audio harness (Windows/MinGW g++). Run from repo root:
#   bash host/build.sh && host/desktop_synth [sample.wav]
# Fetches the vendored single-header miniaudio on first run (not committed).
set -e
cd "$(dirname "$0")/.."

MA=host/vendor/miniaudio.h
if [ ! -f "$MA" ]; then
  echo "fetching miniaudio.h (public domain / MIT-0)..."
  mkdir -p host/vendor
  curl -fsSL https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h -o "$MA"
fi

g++ -std=c++17 -O2 -I src/synth -I host/vendor \
    host/desktop_synth.cpp -o host/desktop_synth \
    -lole32 -lwinmm
echo "built host/desktop_synth"
