#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p build
cc -std=c11 -D_GNU_SOURCE -DSB_WEBOS=1 -Isrc -Ithird_party/cjson \
  tests/phone/address_test.c -lpthread -o build/phone_address_test
build/phone_address_test
for platform in LINUX WEBOS; do
  cc -std=c11 -D_GNU_SOURCE -DSB_${platform}=1 -Isrc -Ithird_party/cjson \
    tests/phone/server_test.c src/core/httpd.c -lpthread -o build/phone_${platform}_test
  build/phone_${platform}_test
  build/phone_${platform}_test occupied
  if [ "$platform" = LINUX ]; then build/phone_${platform}_test configured; fi
done
