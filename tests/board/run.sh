#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p build
cc -std=c11 -D_GNU_SOURCE -DSB_WEBOS=1 -ffunction-sections -fdata-sections \
  -Isrc -Ithird_party/cjson -idirafter "$SDK/usr/include" \
  tests/board/rows_test.c -Wl,--gc-sections -lm -o build/board_rows_test
build/board_rows_test
cc -std=c11 -D_GNU_SOURCE -DSB_WEBOS=1 -ffunction-sections -fdata-sections \
  -Isrc -Ithird_party/cjson -idirafter "$SDK/usr/include" \
  tests/board/nav_test.c -Wl,--gc-sections -lm -o build/board_nav_test
build/board_nav_test
cc -std=c11 -D_GNU_SOURCE -DSB_WEBOS=1 -ffunction-sections -fdata-sections \
  -Isrc -Ithird_party/cjson -idirafter "$SDK/usr/include" \
  tests/board/catalogs_test.c src/core/stremio.c src/core/addons.c third_party/cjson/cJSON.c \
  -Wl,--gc-sections -lm -o build/catalog_defs_test
build/catalog_defs_test
