#!/bin/bash
# Darkwatch (macOS, debug build). Double-click, or run from Terminal.
# Every run writes logs/darkwatch_<date>_<time>.log beside this file: the console output, crash reports
# included. The kernel log (xbox_kernel.log) and indirect-call dumps of the run land in the same folder.
# Game files: a game_files folder beside this file (default.xbe and the .pck files), or game_dir= in
# darkwatch.ini, or DARKWATCH_GAME_DIR.
HERE=$(cd "$(dirname "$0")" && pwd)
LOGS=$HERE/logs
mkdir -p "$LOGS"
LOG=$LOGS/darkwatch_$(date +%Y%m%d_%H%M%S).log
echo "log: $LOG"
cd "$LOGS"
"$HERE/darkwatch_recomp" "$@" 2>&1 | tee "$LOG"
echo "exit ${PIPESTATUS[0]}; log: $LOG"
