#!/usr/bin/env bash
# CTest launcher for tests that open a window.
#   - a real display is present ($DISPLAY or $WAYLAND_DISPLAY): run the command directly
#   - otherwise, if Xvfb + openbox are installed: run inside a private virtual display
#   - otherwise: exit 77, which CTest reports as SKIPPED (never as passed)
set -euo pipefail
if [ -n "${DISPLAY:-}" ] || [ -n "${WAYLAND_DISPLAY:-}" ]; then
  exec "$@"
fi
if command -v Xvfb > /dev/null && command -v openbox > /dev/null; then
  exec "$(dirname "$0")/with_virtual_display.sh" "$@"
fi
echo "no display and no Xvfb/openbox: test skipped (not passed)" >&2
exit 77
