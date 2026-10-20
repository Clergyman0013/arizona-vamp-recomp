#!/usr/bin/env bash
#
# Build Darkwatch Recompiled on macOS (or Linux).
#
#   scripts/build_macos.sh            # configure + build
#   scripts/build_macos.sh --run      # ... then launch the game
#
# Needs: Xcode command line tools, CMake, Homebrew, and SDL2 + libepoxy.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN=0
[ "${1:-}" = "--run" ] && RUN=1

echo "== dependencies =="
if command -v brew >/dev/null 2>&1; then
    for f in cmake sdl2 libepoxy; do
        if brew list --formula "$f" >/dev/null 2>&1; then
            echo "  $f: ok"
        else
            echo "  installing $f"
            brew install "$f"
        fi
    done
else
    echo "Homebrew not found; assuming cmake, SDL2 and libepoxy are installed."
fi

echo
echo "== configure =="
cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release

echo
echo "== build =="
cmake --build "$ROOT/build" --parallel "$(sysctl -n hw.ncpu 2>/dev/null || nproc)"

BIN="$ROOT/build/darkwatch_recomp"
if [ ! -x "$BIN" ]; then
    echo "build did not produce $BIN" >&2
    exit 1
fi

echo
echo "== built: $BIN =="

if [ "$RUN" = "1" ]; then
    echo "== running =="
    cd "$ROOT"
    exec "$BIN"
fi

cat <<EOF

Run it from the project root so the game data is found:

  cd "$ROOT"
  ./build/darkwatch_recomp

Options live in darkwatch.ini (copy config/darkwatch.ini next to the
executable or into the working directory) or in the environment:

  DARKWATCH_WIDESCREEN=1 DARKWATCH_RESOLUTION_SCALE=4 ./build/darkwatch_recomp
EOF
