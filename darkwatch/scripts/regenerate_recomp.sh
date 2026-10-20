#!/usr/bin/env bash
#
# Regenerate the recompiled C from game_files/default.xbe.
#
# Runs the whole xboxrecomp pipeline in order, exactly as docs/GETTING_STARTED
# describes it: parse -> disasm -> func_id -> abi_analysis -> recomp.
#
# Output lands in src/recomp/gen/ (143 files, ~133 MB).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLKIT="$ROOT/tools/xboxrecomp"
[ -d "$TOOLKIT" ] || TOOLKIT="$ROOT/../tools/xboxrecomp"
PY="$ROOT/.venv/bin/python"
[ -x "$PY" ] || PY="$ROOT/../.venv/bin/python"
XBE="$ROOT/game_files/default.xbe"
[ -f "$XBE" ] || XBE="$ROOT/../game_files/default.xbe"
GEN="$ROOT/src/recomp/gen"

if [ ! -x "$PY" ]; then
    echo "Python venv missing. Run scripts/prepare_game_data.sh first." >&2
    exit 1
fi
if [ ! -f "$XBE" ]; then
    echo "Missing $XBE -- run scripts/prepare_game_data.sh first." >&2
    exit 1
fi

cd "$TOOLKIT"

echo "== 1/5  parsing XBE =="
"$PY" -m tools.xbe_parser "$XBE" --json "$(dirname "$XBE")/default_analysis.json"

echo "== 2/5  disassembling (all code sections) =="
"$PY" -m tools.disasm "$XBE" -v

echo "== 3/5  identifying library functions =="
"$PY" -m tools.func_id "$XBE" -v

echo "== 4/5  recovering calling conventions =="
"$PY" -m tools.abi_analysis "$XBE" -v

echo "== 5/5  lifting to C =="
mkdir -p "$GEN"
"$PY" -m tools.recomp "$XBE" --all --split 250 --game-name Darkwatch --gen-dir "$GEN"

echo
echo "== generated =="
ls "$GEN" | head -5
echo "... $(ls "$GEN" | wc -l | tr -d ' ') files, $(du -sh "$GEN" | cut -f1)"
