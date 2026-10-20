#!/bin/bash
# package_win.sh: zip the cross-built Windows exe for a release (no game data).
#   tools/package_win.sh [build type, default Debug] [name]   -> out/release/<name>.zip
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
SDK=$(cd "$HERE/../third_party/XboxXBERecomp" && pwd)
TYPE=${1:-Debug}; LOW=$(echo "$TYPE" | tr '[:upper:]' '[:lower:]')
NAME=${2:-Darkwatch-windows-x64-$LOW}
BUILD=$HERE/out/win-$LOW; OUT=$HERE/out/release/$NAME
rm -rf "$OUT" "$OUT.zip"; mkdir -p "$OUT/assets"
cp "$BUILD/Darkwatch.exe" "$OUT/"
[ -f "$BUILD/Darkwatch.pdb" ] && cp "$BUILD/Darkwatch.pdb" "$OUT/"
cp "$HERE/tools/README-release-windows.txt" "$OUT/README.txt"
cp "$SDK/THIRD_PARTY.txt" "$SDK/LICENSE" "$OUT/"
mv "$OUT/LICENSE" "$OUT/LICENSE-XboxXBERecomp.txt"
(cd "$OUT/.." && zip -qr "$NAME.zip" "$NAME")
echo "packaged $OUT.zip"
