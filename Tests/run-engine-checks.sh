#!/bin/bash
# Builds and runs Tests/EngineChecks.cpp: the reverb engine on its own, without JUCE.
# Needs nothing but g++ (or any C++17 compiler: set CXX).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${TMPDIR:-/tmp}/cloudveil-engine-checks"
${CXX:-g++} -std=c++17 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion "$HERE/EngineChecks.cpp" -o "$OUT"
"$OUT"
