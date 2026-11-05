#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p build
cc -std=c11 -D_GNU_SOURCE -DSB_WEBOS=1 -ffunction-sections -fdata-sections \
  -Isrc -Ithird_party/cjson -idirafter "$SDK/usr/include" \
  tests/metadata/row_test.c src/ui/widgets.c src/core/stremio.c \
  third_party/cjson/cJSON.c -Wl,--gc-sections -lm -o build/metadata_row_test
build/metadata_row_test
cc -std=c11 -D_GNU_SOURCE -DSB_WEBOS=1 -ffunction-sections -fdata-sections \
  -Isrc -Ithird_party/cjson -idirafter "$SDK/usr/include" \
  tests/metadata/hero_test.c third_party/cjson/cJSON.c -Wl,--gc-sections -lm -o build/metadata_hero_test
build/metadata_hero_test
