#!/bin/bash
# build_win_cross.sh: Darkwatch.exe for Windows x64, built on macOS (or Linux) with clang + lld-link + xwin.
#   tools/build_win_cross.sh [Debug|Release|RelWithDebInfo]   (default Debug)
# Needs: brew install llvm lld xwin cmake ninja
#        xwin --accept-license --arch x86_64 splat --output ~/.xwin-cache/sdk
# The recompiler runs on this machine (out/host, tools/host/CMakeLists.txt); the game and runtime are built for
# Windows into out/win-<type>. A Debug exe writes a numbered, verbose log per run to logs\ beside it.
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
TYPE=${1:-Debug}
OUT=$HERE/out/win-$(echo "$TYPE" | tr '[:upper:]' '[:lower:]')
[ -f "$HERE/assets/default.xbe" ] || { echo "put the game files in $HERE/assets first"; exit 1; }
cmake -S "$HERE/tools/host" -B "$HERE/out/host" -G Ninja -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build "$HERE/out/host" --target xbe_recomp
cmake -S "$HERE" -B "$OUT" -G Ninja -DCMAKE_BUILD_TYPE="$TYPE" \
    -DCMAKE_TOOLCHAIN_FILE="$HERE/tools/windows-cross.cmake" \
    -DXBE_RECOMP_EXECUTABLE="$HERE/out/host/xbe_recomp"
cmake --build "$OUT" --target Darkwatch
echo "built $OUT/Darkwatch.exe"
