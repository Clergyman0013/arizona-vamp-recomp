#!/usr/bin/env bash
#
# Run the title with the bring-up switches that get it furthest today.
#
#   scripts/run_bringup.sh            # run until Ctrl-C
#   scripts/run_bringup.sh --log FILE # also tee everything to FILE
#
# Ctrl-C stops the run and writes icall_targets.dump (and the DARKWATCH_PROBE
# dump, if set) -- that is the point of running it this way rather than from a
# debugger: a hang is a data point, and the dump is how it is collected.
#
# What each switch does, and where the title currently stops, is in
# ../BRINGUP.md.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/build/darkwatch_recomp"
[ -x "$BIN" ] || { echo "Build first: scripts/build_macos.sh"; exit 1; }

LOG=""
if [ "${1:-}" = "--log" ] && [ -n "${2:-}" ]; then LOG="$2"; fi

# Execute the title's pushbuffers, deliver the GPU interrupt, and recognise the
# events it builds itself. The GPU fence (PGRAPH 0xFD400B10 + the semaphore at
# *pb_gpu_read_ptr) is implemented in the executor and needs no switch.
export RECOMP_PB_EXEC=1
export RECOMP_VBLANK=1
export RECOMP_TITLE_KEVENTS=1
export XBOX_LOG_LEVEL="${XBOX_LOG_LEVEL:-3}"

# Legacy bring-up lever: satisfy a fence wait outright instead of through the
# fence protocol. Only useful to prove the fence path is the thing blocking.
# export RECOMP_GPU_IDLE_FENCE=1

# Writes every presented frame to a BMP without needing a window.
# export RECOMP_FB_DUMP_FLIPS=1

# Where the run is stuck, in one line each time it is stopped:
#   [0x3548F8] holds the D3D device context; +0x2C is pb_write_seq, +0x30 the
#   read-position pointer, +0x1DCC the stall event. Uncomment to dump them.
# export DARKWATCH_PROBE=0x354900,+0x30

cd "$ROOT"
if [ -n "$LOG" ]; then
    exec "$BIN" 2>&1 | tee "$LOG"
else
    exec "$BIN"
fi
