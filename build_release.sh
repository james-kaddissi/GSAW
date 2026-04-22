#!/bin/bash
set -e

BUILD_DIR="build"
CONFIG="Release"

if [ ! -d "$BUILD_DIR" ]; then
  echo "Build directory not found. Configuring..."
  cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIG"
fi

cmake --build "$BUILD_DIR" --config "$CONFIG"

SRC="$PWD/$BUILD_DIR/gs-engine-build/$CONFIG"
DST="$PWD/$BUILD_DIR/$CONFIG"

mkdir -p "$DST"

for file in "$SRC"/*.dylib; do
  [ -e "$file" ] || continue
  cp -f "$file" "$DST"/
done

echo "Done."