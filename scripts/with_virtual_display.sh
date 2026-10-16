#!/usr/bin/env bash
# Runs a command inside a private Xvfb display with the openbox window manager (so minimize /
# restore / resize requests are honoured as on a desktop). Used on headless Linux hosts only;
# on a workstation with a real display, run the command directly.
#   scripts/with_virtual_display.sh <command> [args...]
set -euo pipefail
for tool in Xvfb openbox; do
  command -v "$tool" >/dev/null || { echo "with_virtual_display: $tool is not installed" >&2; exit 77; }
done
display_num=$(( 90 + RANDOM % 900 ))
Xvfb ":${display_num}" -screen 0 1920x1080x24 -nolisten tcp > /dev/null 2>&1 &
xvfb_pid=$!
cleanup() { kill "${wm_pid:-}" "$xvfb_pid" 2>/dev/null || true; wait 2>/dev/null || true; }
trap cleanup EXIT
export DISPLAY=":${display_num}"
for _ in $(seq 1 50); do xdpyinfo > /dev/null 2>&1 && break; sleep 0.1; done
openbox > /dev/null 2>&1 &
wm_pid=$!
sleep 0.5
"$@"
