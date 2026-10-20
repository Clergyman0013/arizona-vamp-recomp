#!/usr/bin/env bash
#
# Extract the game data from your Darkwatch disc image into game_files/.
#
#   scripts/prepare_game_data.sh "/path/to/Darkwatch (USA).xiso.iso"
#
# Uses the toolkit's own XDVDFS reader (tools/xiso), so no external tool is
# needed. Everything goes to ./game_files next to the project, which is one of
# the locations the bootstrap searches.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ISO="${1:-}"

if [ -z "$ISO" ]; then
    # Default to the disc image this project was built against, if present.
    for cand in "$ROOT"/*.iso "$ROOT"/*.xiso.iso "$ROOT"/*.xiso; do
        [ -f "$cand" ] && ISO="$cand" && break
    done
fi

if [ -z "$ISO" ] || [ ! -f "$ISO" ]; then
    echo "usage: $0 /path/to/darkwatch.iso" >&2
    exit 2
fi

PY="$ROOT/.venv/bin/python"
if [ ! -x "$PY" ]; then
    echo "== creating python venv (.venv) =="
    python3 -m venv "$ROOT/.venv"
    "$ROOT/.venv/bin/pip" install -q --upgrade pip capstone
fi

TOOLKIT="$ROOT/tools/xboxrecomp"
if [ ! -d "$TOOLKIT" ]; then
    echo "== cloning xboxrecomp toolkit =="
    mkdir -p "$ROOT/tools"
    git clone --depth 1 https://github.com/sp00nznet/xboxrecomp "$TOOLKIT"
fi

echo "== unpacking $ISO =="
mkdir -p "$ROOT/game_files"
"$PY" -m tools.xiso unpack "$ISO" -o "$ROOT/game_files" --yes 2>/dev/null \
    || (cd "$TOOLKIT" && "$PY" -m tools.xiso unpack "$ISO" -o "$ROOT/game_files")

echo
echo "== done =="
ls -1 "$ROOT/game_files" | head -30
echo
echo "default.xbe and the .pck files should be listed above."
