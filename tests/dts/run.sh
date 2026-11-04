#!/bin/bash
# tests/dts/run.sh <file.mkv> [outdir]: converts a file with build/mac/libsbdts.dylib (tools/build-ffmpeg.sh mac) and
# checks the result with the host's ffprobe / ffmpeg: every segment decodes, the timestamps run on across segments,
# the sound stays with the picture, and a segment made after a seek matches the one made in order
set -eu
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; F="$1"; O="${2:-$(mktemp -d)}"; mkdir -p "$O"
cc -std=c11 -D_POSIX_C_SOURCE=200809L -D_FILE_OFFSET_BITS=64 -O1 -I"$ROOT/src" "$ROOT/tests/dts/remux_test.c" -o "$ROOT/build/remux_test"
LIB="${SBDTS_LIB:-$ROOT/build/mac/libsbdts.dylib}"
"$ROOT/build/remux_test" "$LIB" "$F" "$O"
if [ -f "$O/all.ts" ]; then ffmpeg -v error -i "$O/all.ts" -f null - && echo "continuous stream decodes"; exit 0; fi
ffmpeg -v error -allowed_extensions ALL -i "$O/index.m3u8" -f null - && echo "the playlist decodes end to end"
# JSON avoids blank CSV side-data rows being mistaken for the first timestamp.
first_pts() {
  ffprobe -v error -select_streams "$2" -show_entries packet=pts_time -of json "$1" | python3 -c 'import json, sys; print(min(float(p["pts_time"]) for p in json.load(sys.stdin)["packets"] if "pts_time" in p))'
}
# Each sampled segment starts later than the previous one, with sound near video.
prev=-1
for s in 0 1 2 5; do [ -f "$O/$s.ts" ] || continue
  v=$(first_pts "$O/$s.ts" v); a=$(first_pts "$O/$s.ts" a)
  python3 -c 'import sys; v, a, prev = map(float, sys.argv[1:]); assert v > prev and abs(v - a) < 0.25, (v, a, prev)' "$v" "$a" "$prev"
  echo "segment $s: video from $v, audio from $a"; prev=$v; done
# segment 5 again after a seek: the same video packets, the sound on its own clock from there
cp "$O/5.ts" "$O/5.inorder.ts"; "$ROOT/build/remux_test" "$LIB" "$F" "$O" 9 5 >/dev/null
a=$(ffmpeg -v error -i "$O/5.ts" -map 0:v -c copy -f md5 -); b=$(ffmpeg -v error -i "$O/5.inorder.ts" -map 0:v -c copy -f md5 -)
fa=$(first_pts "$O/5.ts" a); oa=$(first_pts "$O/5.inorder.ts" a)
python3 -c 'import sys; a, b = map(float, sys.argv[1:]); assert abs(a - b) < 0.25, (a, b)' "$fa" "$oa"
if [ "$a" = "$b" ]; then
  echo "segment 5 after a seek: same video, audio from $fa"
else
  echo "segment 5 after a seek: VIDEO DIFFERS"; exit 1
fi
