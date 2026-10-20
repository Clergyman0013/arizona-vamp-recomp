#!/usr/bin/env bash
#
# One turn of the indirect-call feedback loop.
#
#   scripts/bringup_iterate.sh            merge + seed + regenerate + build
#   scripts/bringup_iterate.sh --run      ... then run the title for 60s and
#                                         collect a fresh dump
#
# The recompiled dispatch cannot see where a vtable call goes; the runtime
# records every target it observes and dumps them on exit or on Ctrl-C
# (icall_targets.dump). This script folds that dump back in:
#
#   icall_feedback merge  ->  disasm --seed-functions  ->  recomp  ->  build
#
# Each turn turns unresolved stubs into real functions and reaches further
# into the title. The database is cumulative, so turns accumulate rather than
# starting over.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLKIT="$ROOT/tools/xboxrecomp"
[ -d "$TOOLKIT" ] || TOOLKIT="$ROOT/../tools/xboxrecomp"
PY="$ROOT/.venv/bin/python"
[ -x "$PY" ] || PY="$ROOT/../.venv/bin/python"
XBE="$ROOT/game_files/default.xbe"
[ -f "$XBE" ] || XBE="$ROOT/../game_files/default.xbe"
GEN="$ROOT/src/recomp/gen"
RUN=0
[ "${1:-}" = "--run" ] && RUN=1

# The dump is written to the working directory the title was launched from,
# which is usually the checkout root (one level above this project).
for cand in "$ROOT/icall_targets.dump" "$ROOT/../icall_targets.dump" \
            "${DARKWATCH_DUMP:-}"; do
    [ -n "$cand" ] && [ -f "$cand" ] && DUMP="$cand" && break
done
DUMP="${DUMP:-$ROOT/icall_targets.dump}"

cd "$ROOT"

if [ ! -f "$DUMP" ]; then
    echo "No $DUMP yet."
    echo "Run the title first (Ctrl-C stops it and writes the dump), then re-run this."
    exit 1
fi

echo "== 1/5  merge icall feedback =="
cd "$TOOLKIT"
"$PY" -m tools.recomp.icall_feedback merge "$DUMP" | tail -20

echo
echo "== 2/5  re-run function detection with the observed targets as seeds =="
"$PY" -m tools.disasm "$XBE" --seed-functions tools/recomp/output/icall_targets.json | tail -5

echo
echo "== 3/5  re-lift =="
"$PY" -m tools.recomp "$XBE" --all --split 250 --game-name Darkwatch --gen-dir "$GEN" | tail -5

echo
echo "== 4/5  build =="
cmake --build "$ROOT/build" --parallel "$(sysctl -n hw.ncpu 2>/dev/null || nproc)" \
    | grep -E "error|Linking C executable" || true

if [ "$RUN" = "1" ]; then
    echo
    echo "== 5/5  run for 60s (Ctrl-C sooner to stop and dump) =="
    cd "$ROOT"
    "$ROOT/build/darkwatch_recomp" &
    pid=$!
    trap 'kill -TERM $pid 2>/dev/null || true' INT TERM
    sleep 60
    kill -TERM $pid 2>/dev/null || true
    wait $pid 2>/dev/null || true
    echo "dump: $DUMP"
fi
