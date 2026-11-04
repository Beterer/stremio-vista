#!/bin/bash
# Host-side checks that do not require a TV, credentials, or the external web app.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
bash tools/logs-check.sh
cargo test --manifest-path tools/dovi/Cargo.toml --locked
mkdir -p build
cc -std=c11 -D_GNU_SOURCE -DSB_BEST_PURE -Wall -Wextra -Wno-unused-parameter -O1 -g \
  -Isrc -Ithird_party/cjson tests/best/best_test.c src/core/best.c third_party/cjson/cJSON.c -lm -o build/best_test
python3 - <<'PY'
import itertools, json, pathlib, subprocess
caps = [(1080, 0, 0, 0, 1, 0), (2160, 1, 1, 1, 0, 0),
        (2160, 1, 0, 0, 1, 0), (2160, 0, 0, 0, 0, 0)]
cases = 0
for fixture, tv, speed in itertools.product(sorted(pathlib.Path('tests/best/fixtures').glob('*.json')), caps, [0, 12, 30, 80, 250]):
    result = json.loads(subprocess.check_output(['build/best_test', str(fixture), *map(str, tv), str(speed)]))
    pick, rates = result['pick'], result['rates']
    assert -1 <= pick < len(rates), (fixture, tv, speed, pick)
    if pick >= 0:
        assert rates[pick] is not None, (fixture, tv, speed, pick)
    cases += 1
assert cases, 'No scoring fixtures found'
print(f'{cases} scoring smoke cases passed (external web-app parity comparison not run)')
PY
