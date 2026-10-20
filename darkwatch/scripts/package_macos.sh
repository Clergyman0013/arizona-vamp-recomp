#!/bin/bash
# package_macos.sh: a self-contained folder (and zip) of the macOS build for a release.
#   scripts/package_macos.sh [name]   -> build/release/<name>/ and build/release/<name>.zip
# Takes build/darkwatch_recomp as built, copies the Homebrew libraries it links (and SDL3, which sdl2-compat
# loads at run time) into lib/, points the binary at them, and re-signs ad hoc. No game data goes in.
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
NAME=${1:-Darkwatch-macos-arm64-debug}
OUT=$HERE/build/release/$NAME
rm -rf "$OUT" "$OUT.zip"; mkdir -p "$OUT/lib"
cp "$HERE/build/darkwatch_recomp" "$HERE/config/darkwatch.ini" "$HERE/scripts/Darkwatch.command" "$OUT/"
cp "$HERE/scripts/README-release-macos.txt" "$OUT/README.txt"
for lib in $(otool -L "$OUT/darkwatch_recomp" | awk '/\/opt\/homebrew\//{print $1}'); do
    cp -L "$lib" "$OUT/lib/"
    install_name_tool -change "$lib" "@executable_path/lib/$(basename "$lib")" "$OUT/darkwatch_recomp"
done
cp -L "$(brew --prefix sdl3)/lib/libSDL3.0.dylib" "$OUT/lib/"
chmod u+w "$OUT"/lib/*
for f in "$OUT"/lib/*.dylib; do install_name_tool -id "@executable_path/lib/$(basename "$f")" "$f"; done
codesign --force -s - "$OUT"/lib/*.dylib "$OUT/darkwatch_recomp"
(cd "$OUT/.." && ditto -c -k --keepParent "$NAME" "$NAME.zip")
echo "packaged $OUT.zip"
