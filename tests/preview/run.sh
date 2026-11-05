#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p build
read -r -a sdl_cflags <<< "$(pkg-config --cflags sdl2)"
read -r -a sdl_libs <<< "$(pkg-config --libs sdl2)"
cc -std=c11 -D_GNU_SOURCE -DSB_LINUX=1 -ffunction-sections -fdata-sections \
  -Isrc -Ithird_party/cjson "${sdl_cflags[@]}" \
  tests/preview/platform_test.c src/platform/linux/plat_linux.c src/core/store.c \
  third_party/cjson/cJSON.c -Wl,--gc-sections "${sdl_libs[@]}" -lpthread -lm -o build/preview_platform_test
build/preview_platform_test
