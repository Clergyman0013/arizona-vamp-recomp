#!/bin/bash
# run_mac.sh: one headless run of the harness built by harness_mac.sh
#   tools/run_mac.sh <name> <seconds> [pad script]
# Runs in out/<name> against ../game_files, stops after <seconds>, then prints the log's errors and tail.
# env passes through: REF=N (reference frames into cap/), CAP=N (draw lists), XBE_PERF ...
HERE=$(cd "$(dirname "$0")/.." && pwd)
DIR=$HERE/out/$1; T=${2:-60}
rm -rf "$DIR"; mkdir -p "$DIR"; cd "$DIR" || exit 1
[ -n "$3" ] && export SCRIPT=$(cd "$(dirname "$3")" && pwd)/$(basename "$3")
"$HERE/out/harness/darkwatch" "$HERE/../game_files" > run.out 2>&1 &
pid=$!
for _ in $(seq "$T"); do kill -0 $pid 2>/dev/null || break; sleep 1; done
if kill -0 $pid 2>/dev/null; then kill $pid; wait $pid 2>/dev/null; echo "stopped after ${T}s"
else wait $pid; echo "exited $?"; fi
grep -E '\[(error|warn)\]' run.out | sed 's/^\[ *[0-9.]*\] //;s/\[t[0-9a-f]*\] //' | sort | uniq -c | sort -rn | head -20
tail -6 run.out | cut -c1-200
ls cap 2>/dev/null | tail -3
