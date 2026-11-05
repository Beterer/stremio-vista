#!/bin/bash
# Run the native desktop UI behind a browser-accessible, container-local display.
set -euo pipefail
cd "$(dirname "$0")/.."
bash tools/build.sh linux
mkdir -p "$SB_DATA_DIR"
if [ "${SB_PREVIEW_DEMO:-0}" = 1 ]; then
  python3 tools/preview-demo.py "$SB_DATA_DIR"
  export SB_SKIP_LOGIN=1
fi

pids=()
cleanup() { if [ "${#pids[@]}" -gt 0 ]; then kill "${pids[@]}" 2>/dev/null || true; wait "${pids[@]}" 2>/dev/null || true; fi; }
trap cleanup EXIT
trap 'exit 143' TERM
trap 'exit 130' INT
Xvfb "$DISPLAY" -screen 0 "${SB_WINDOW}x24" -nolisten tcp > /tmp/vista-xvfb.log 2>&1 &
pids+=("$!")
for ((i=0; i<50; i++)); do
  if xdpyinfo -display "$DISPLAY" >/dev/null 2>&1; then break; fi
  sleep 0.1
done
xdpyinfo -display "$DISPLAY" >/dev/null
x11vnc -display "$DISPLAY" -localhost -rfbport 5900 -nopw -forever -shared -noxdamage -quiet > /tmp/vista-vnc.log 2>&1 &
pids+=("$!")
websockify --web /usr/share/novnc 6080 localhost:5900 > /tmp/vista-websockify.log 2>&1 &
pids+=("$!")
build/linux/stremioorigin "$PWD/assets" > >(tee /tmp/vista-preview.log) 2>&1 &
pids+=("$!")
wait -n "${pids[@]}"
