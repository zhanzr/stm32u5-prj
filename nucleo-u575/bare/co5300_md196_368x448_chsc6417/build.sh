#!/usr/bin/env bash
# Build the STM32U575ZIT6 "nucleo-u575" co5300_md196_368x448_chsc6417 project with CMake + Ninja
# (Pico-style). Run with:  bash build.sh    (or ./build.sh on Linux)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ---------------------------------------------------------------------------
# Tool selection.
#
# On Windows/MSYS2 build with the **native mingw64** CMake + Ninja. The MSYS2
# CMake writes POSIX paths (e.g. /usr/bin/cmake.exe) into build.ninja, which
# only MSYS2-aware Ninja can execute; a native Ninja (as found in a MINGW64
# shell or a standalone install) then fails with:
#     CreateProcess failed: The system cannot find the file specified.
# Native CMake emits plain Windows paths, so the build tree it produces works
# from any shell/terminal.
NINJA_BIN=ninja
if [ -x /mingw64/bin/ninja.exe ]; then
    export PATH="/mingw64/bin:$PATH"
    NINJA_BIN=/mingw64/bin/ninja.exe
elif [ -d /mingw64/bin ] && ! command -v ninja >/dev/null 2>&1; then
    export PATH="/mingw64/bin:/usr/bin:$PATH"
fi
if [ -x /mingw64/bin/cmake.exe ]; then
    CMAKE_BIN=/mingw64/bin/cmake.exe
else
    CMAKE_BIN=cmake
fi

BUILD_DIR="${BUILD_DIR:-build}"
mkdir -p "$BUILD_DIR"
"$CMAKE_BIN" -G Ninja -S . -B "$BUILD_DIR" "$@"
"$NINJA_BIN" -C "$BUILD_DIR"
