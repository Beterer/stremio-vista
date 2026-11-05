#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p build
cc -std=c11 -D_GNU_SOURCE -DSB_WEBOS=1 -ffunction-sections -fdata-sections \
  -Isrc -Ithird_party/cjson -idirafter "$SDK/usr/include" \
  tests/update/lookup_test.c third_party/cjson/cJSON.c -Wl,--gc-sections -lm -o build/update_lookup_test
build/update_lookup_test
