#!/usr/bin/env bash
# G0-04: the committed tree (not the working copy) configures, builds and passes its tests from a
# fresh clone with fresh dependency fetches. DXC is the source-built compiler from this checkout's
# build/tools (a fresh clone has none; building it again takes ~16 min, see ADR-015).
#   scripts/fresh_clone_evidence.sh <scratch-dir>
# Logs go to docs/reports/evidence/logs/fresh_clone_*.log with the clone and repository paths
# replaced by <fresh-clone> and <repo>.
set -uo pipefail
repo="$(cd "$(dirname "$0")/.." && pwd)"
scratch="$1"
L="$repo/docs/reports/evidence/logs"
mkdir -p "$L" "$scratch"
clone="$(cd "$scratch" && pwd)/fresh-clone"
rm -rf "$clone"
dxc="$(ls -d "$repo"/build/tools/dxc-*/bin/dxc | head -1)"
run() {  # run <log> <cmd...>   (appends)
  local log="$1"; shift
  echo "\$ $*" >> "$log"
  (cd "$clone" 2>/dev/null || cd "$scratch"; "$@") >> "$log" 2>&1
  local code=$?
  echo "# exit $code" >> "$log"
  return $code
}
H="$L/fresh_clone_dev_headless.log"
V="$L/fresh_clone_dev_vulkan.log"
: > "$H"; : > "$V"
echo "# $(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$H"
run "$H" git clone --no-local "$repo" "$clone" || exit 1
run "$H" git log -1 --format='clone HEAD %H %s' HEAD
run "$H" git status --porcelain
rc=0
( unset DISPLAY WAYLAND_DISPLAY
  run "$H" cmake --preset dev-headless && run "$H" cmake --build --preset dev-headless && run "$H" ctest --preset dev-headless ) || rc=1
echo "# $(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$V"
{ run "$V" cmake --preset dev-vulkan -DSOV_DXC_EXECUTABLE="$dxc" && run "$V" cmake --build --preset dev-vulkan &&
  run "$V" ctest --preset dev-vulkan --output-on-failure; } || rc=1
sed -i -e "s#$clone#<fresh-clone>#g" -e "s#$repo#<repo>#g" "$H" "$V"
rm -rf "$clone"
echo "fresh clone: headless $(tail -1 "$H"), vulkan $(tail -1 "$V")"
exit $rc
