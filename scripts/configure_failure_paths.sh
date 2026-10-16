#!/usr/bin/env bash
# G0-07 (configure side): a missing or unusable prerequisite stops the configure step with a
# diagnostic and a nonzero exit; nothing falls back silently.
#   scripts/configure_failure_paths.sh <out-dir>
# Cases that need the dependency sources reuse build/dev-vulkan/_deps/*-src through
# FETCHCONTENT_SOURCE_DIR_<NAME> and run with networking disabled (unshare -rn), so they prove the
# check fires without any download. Requires a previous dev-vulkan configure.
set -uo pipefail
src="$(cd "$(dirname "$0")/.." && pwd)"
out="$1"
mkdir -p "$out"
out="$(cd "$out" && pwd)"
work="$out/work"
rm -rf "$work"
mkdir -p "$work"
deps="$src/build/dev-vulkan/_deps"
[ -d "$deps" ] || { echo "configure_failure_paths: run 'cmake --preset dev-vulkan' first ($deps missing)" >&2; exit 2; }
command -v unshare > /dev/null && unshare -rn true 2> /dev/null && offline=(unshare -rn) || offline=()

reuse=()
for d in "$deps"/*-src; do
  n=$(basename "$d" -src | tr '[:lower:]' '[:upper:]')
  reuse+=("-DFETCHCONTENT_SOURCE_DIR_${n}=$d")
done

failures=0
results=()
# case <name> <required diagnostic (ERE)> <command...>   (expects a nonzero exit)
case_() {
  local name="$1" diag="$2"
  shift 2
  local log="$out/$name.log" code verdict="passed" why=""
  echo "\$ $*" > "$log"
  timeout 600 "$@" >> "$log" 2>&1
  code=$?
  if [ "$code" -eq 0 ]; then
    verdict="failed"; why="configure succeeded (silent fallback)"
  elif [ "$code" -eq 124 ]; then
    verdict="failed"; why="timed out"
  elif ! sed 1d "$log" | grep -q 'CMake Error'; then
    verdict="failed"; why="nonzero exit without a CMake Error diagnostic"
  elif ! sed 1d "$log" | tr '\n' ' ' | tr -s ' ' | grep -Eq -- "$diag"; then
    # CMake wraps messages; match against the joined text.
    verdict="failed"; why="diagnostic /$diag/ not found"
  fi
  echo "# exit $code: $verdict $why" >> "$log"
  printf '%-44s exit %-3s %s %s\n' "$name" "$code" "$verdict" "$why"
  [ "$verdict" = "passed" ] || failures=$((failures + 1))
  results+=("$(printf '{"case":"%s","exit":%s,"verdict":"%s","reason":"%s","network":"%s","log":"%s.log"}' \
    "$name" "$code" "$verdict" "$why" "$([ ${#offline[@]} -gt 0 ] && echo disabled || echo available)" "$name")")
}

cd "$src"
case_ dxc_override_does_not_exist 'does not exist or is not a file' \
  "${offline[@]}" cmake --preset dev-vulkan -B "$work/dxc_missing" "${reuse[@]}" -DSOV_DXC_EXECUTABLE="$work/no-such-dxc"
case_ dxc_override_is_a_directory 'does not exist or is not a file' \
  "${offline[@]}" cmake --preset dev-vulkan -B "$work/dxc_dir" "${reuse[@]}" -DSOV_DXC_EXECUTABLE="$work"
printf '#!/bin/sh\necho "this is not dxc" >&2\nexit 3\n' > "$work/broken-dxc"
chmod +x "$work/broken-dxc"
case_ dxc_override_cannot_run 'cannot run' \
  "${offline[@]}" cmake --preset dev-vulkan -B "$work/dxc_broken" "${reuse[@]}" -DSOV_DXC_EXECUTABLE="$work/broken-dxc"
# A dependency checkout at the wrong revision is refused (G0-03 pin verification).
wrong=("${reuse[@]/#-DFETCHCONTENT_SOURCE_DIR_IMGUI=*/-DFETCHCONTENT_SOURCE_DIR_IMGUI=$deps/volk-src}")
case_ dependency_at_unpinned_revision 'Refusing to build with an unverified revision' \
  "${offline[@]}" cmake --preset dev-vulkan -B "$work/wrong_rev" "${wrong[@]}"
# A fresh configure with no dependency cache and no network fails at the first fetch.
if [ ${#offline[@]} -gt 0 ]; then
  case_ offline_configure_without_dependency_cache "downloading '[^']*' failed|Could not resolve host|unable to access" \
    "${offline[@]}" cmake --preset dev-headless -B "$work/offline_fresh"
fi

{
  echo '{'
  echo '  "tool": "scripts/configure_failure_paths.sh",'
  echo "  \"commit\": \"$(git -C "$src" rev-parse HEAD 2>/dev/null || echo unknown)$(git -C "$src" diff --quiet HEAD 2>/dev/null || echo ' (modified)')\","
  echo "  \"failures\": $failures,"
  echo '  "cases": ['
  for i in "${!results[@]}"; do
    sep=','; [ "$i" -eq $(( ${#results[@]} - 1 )) ] && sep=''
    echo "    ${results[$i]}$sep"
  done
  echo '  ]'
  echo '}'
} > "$out/configure_failure_paths.json"
rm -rf "$work"
echo "configure failure paths: $(( ${#results[@]} - failures ))/${#results[@]} passed"
[ "$failures" -eq 0 ]
