#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
case "${1:-doctor}" in
  doctor)
    zig version
    cc --version | head -1
    cmake --version | head -1
    ninja --version
    python3 --version
    node --version
    rustc --version
    rustup target list --installed
    ares-package --version
    test -f "$SDK/usr/lib/libSDL2.so"
    test -f "$SDK/usr/include/SDL2/SDL.h"
    echo "webOS sysroot found"
    ;;
  libraries)
    ENGINE_JOBS="${ENGINE_JOBS:-2}" bash tools/build-engine.sh webos
    bash tools/build-ffmpeg.sh webos
    ;;
  test-media)
    bash tools/build-ffmpeg.sh linux
    mkdir -p build/media-test
    ffmpeg -hide_banner -loglevel error -y \
      -f lavfi -i testsrc2=size=320x180:rate=24 -f lavfi -i sine=frequency=440:sample_rate=48000 \
      -t 24 -c:v libx264 -g 48 -pix_fmt yuv420p -c:a dca -strict -2 build/media-test/dts.mkv
    SBDTS_LIB="$PWD/build/linux/libsbdts.so" bash tests/dts/run.sh "$PWD/build/media-test/dts.mkv" "$PWD/build/media-test/segments"
    ;;
  *) echo "unknown command: $1" >&2; exit 2 ;;
esac
