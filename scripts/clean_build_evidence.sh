#!/usr/bin/env bash
# Clean-checkout style build of both presets with fresh dependency fetches; logs per step.
cd "$(dirname "$0")/.."
L=docs/reports/evidence/logs
mkdir -p "$L"
rm -rf build/dev-headless build/dev-vulkan build/dev-headless-asan
run() { # run <log-name> <cmd...>
  local name="$1"; shift
  { echo "\$ $*"; echo "# started $(date -u +%Y-%m-%dT%H:%M:%SZ) at commit $(git rev-parse HEAD)"; } > "$L/$name.log"
  local t0=$(date +%s)
  "$@" >> "$L/$name.log" 2>&1; local code=$?
  echo "# finished $(date -u +%Y-%m-%dT%H:%M:%SZ) exit $code after $(( $(date +%s) - t0 ))s" >> "$L/$name.log"
  echo "$name exit $code ($(( $(date +%s) - t0 ))s)" | tee -a "$L/clean_build_summary.txt"
  return $code
}
: > "$L/clean_build_summary.txt"
run clean_dev_headless_1_configure cmake --preset dev-headless || exit 1
run clean_dev_headless_2_build cmake --build --preset dev-headless || exit 1
( unset DISPLAY WAYLAND_DISPLAY; run clean_dev_headless_3_test ctest --preset dev-headless --output-on-failure ) || exit 1
run clean_dev_vulkan_1_configure cmake --preset dev-vulkan || exit 1
run clean_dev_vulkan_2_build cmake --build --preset dev-vulkan || exit 1
run clean_dev_vulkan_3_test ctest --preset dev-vulkan --output-on-failure
echo "ALL DONE" >> "$L/clean_build_summary.txt"
