#!/bin/bash
# harness_mac.sh: the SDK's headless harness (sdk/harness/build.sh) built natively on macOS.
#   tools/harness_mac.sh [out folder, default out/harness]
# Same parts as the Linux script: the recompiled code, the runtime without the window, SDL input, XAudio2 and
# D3D12, and the harness stand ins. Differs only where macOS does: no -march (arm64 hosts), sysctl for the job
# count, BSD tools, and paths with spaces in them.
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
SDK=$(cd "$HERE/../third_party/XboxXBERecomp" && pwd)
GEN=$HERE/generated/game
OUT=${1:-$HERE/out/harness}
mkdir -p "$OUT/gen" "$OUT/host"
H=$SDK/harness; RT=$SDK/runtime
CXX=${CXX:-clang++}; JOBS=${JOBS:-$(sysctl -n hw.ncpu)}
BASE=(-std=c++20 -fno-strict-aliasing)
HOST=("${BASE[@]}" -O1 -g "-I$HERE/src" "-I$RT" "-I$GEN" "-I$H" "-I$H/compat" -DXBE_RAW_VERTICES=0 -DXBE_RENDER_THREAD=0)

throttle() { while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do sleep 0.2; done; }

# generated code, an object rebuilt only when its partition is newer
for f in "$GEN"/x86_recomp.*.cpp "$GEN"/x86_func_mapping.cpp; do
    o=$OUT/gen/$(basename "$f" .cpp).o
    [ "$o" -nt "$f" ] && continue
    ( "$CXX" "${BASE[@]}" -O1 -g0 -w "-I$RT" "-I$GEN" -c "$f" -o "$o.tmp" && mv "$o.tmp" "$o" ) &
    throttle
done
wait

SRC=()
while IFS= read -r f; do SRC+=("$f"); done < <(
    ls "$RT"/*/*.cpp | grep -Ev "/runtime/app/|/core/input.cpp|/gpu/d3d12.cpp|/audio/xaudio2.cpp"
    ls "$HERE"/src/*/*.cpp "$H"/*.cpp)

stamp=$(printf '%s ' "${HOST[@]}" | md5 -q | cut -c1-8)
newest=$( { find "$RT" "$HERE/src" "$H" -name '*.h'; echo "$GEN/image_symbols.h"; } | tr '\n' '\0' |
          xargs -0 ls -t | head -1)
objs=()
for f in "${SRC[@]}"; do
    o=$OUT/host/$(echo "${f#/}" | tr '/ ' '__' | sed "s/\.cpp$/.$stamp.o/"); objs+=("$o")
    [ "$o" -nt "$f" ] && [ "$o" -nt "$newest" ] && continue
    echo "compile $f"
    rm -f "$o"
    "$CXX" "${HOST[@]}" -c "$f" -o "$o" &
    throttle
done
wait
"$CXX" -o "$OUT/darkwatch" "${objs[@]}" "$OUT"/gen/*.o -lpthread
echo "built $OUT/darkwatch"
