#!/usr/bin/env bash
# G1-R14 / G0-07: normal close and failed initialization unwind cleanly; every missing prerequisite
# gives a diagnostic and a specific nonzero exit, never a silent fallback, crash or hang.
#   scripts/failure_paths_check.sh <bin-dir> <out-dir>
# Needs a display for the cases that get as far as a window (run it through
# scripts/ctest_display_launcher.sh). Each case runs under a 120 s timeout; a timeout is a failure
# (deadlock), as is any validation message, sanitizer report or crash signature in its output.
# Writes <out-dir>/<case>.log and <out-dir>/failure_paths.json; exits 1 if any case failed.
set -uo pipefail
bin="$(cd "$1" && pwd)"
out="$2"
src="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$out"
out="$(cd "$out" && pwd)"
work="$out/work"
rm -rf "$work"
mkdir -p "$work"
scene="$src/content/authoring/first_light.scene.json"
cli="$bin/sovereign-cli"

results=()
failures=0
# case <name> <expected exit> <required diagnostic (ERE)> <command...>
case_() {
  local name="$1" want="$2" diag="$3"
  shift 3
  local log="$out/$name.log" t0 t1 code verdict="passed" why=""
  { echo "\$ $*"; echo "# expected exit $want, diagnostic /$diag/"; } > "$log"
  t0=$(date +%s.%N)
  timeout --kill-after=10 120 "$@" >> "$log" 2>&1
  code=$?
  t1=$(date +%s.%N)
  if [ "$code" -eq 124 ] || [ "$code" -eq 137 ]; then
    verdict="failed"; why="timed out (hang or deadlock)"
  elif [ "$code" -ne "$want" ]; then
    verdict="failed"; why="exit $code, expected $want"
  elif ! sed 1,2d "$log" | grep -Eq -- "$diag"; then
    verdict="failed"; why="diagnostic /$diag/ not found"
  elif sed 1,2d "$log" | grep -Eq 'VUID-|vulkan-validation\]|AddressSanitizer|LeakSanitizer|runtime error:|Segmentation fault|core dumped|terminate called|Assertion .* failed' "$log"; then
    verdict="failed"; why="validation, sanitizer or crash output: $(sed 1,2d "$log" | grep -Em1 'VUID-|vulkan-validation\]|AddressSanitizer|LeakSanitizer|runtime error:|Segmentation fault|core dumped|terminate called|Assertion .* failed' "$log" | cut -c1-200)"
  fi
  printf '# exit %s after %.2fs: %s %s\n' "$code" "$(echo "$t1 - $t0" | bc)" "$verdict" "$why" >> "$log"
  printf '%-48s exit %-3s %s %s\n' "$name" "$code" "$verdict" "$why"
  [ "$verdict" = "passed" ] || failures=$((failures + 1))
  results+=("$(printf '{"case":"%s","expected_exit":%s,"exit":%s,"seconds":%.2f,"verdict":"%s","reason":"%s","log":"%s.log"}' \
    "$name" "$want" "$code" "$(echo "$t1 - $t0" | bc)" "$verdict" "${why//\"/\'}" "$name")")
}

# --- normal close -------------------------------------------------------------------------------
case_ runtime_normal_close 0 'validation: layer on, sync on, errors 0' \
  "$bin/sovereign-runtime" --scene "$scene" --frames 30

# --- scene prerequisites ------------------------------------------------------------------------
case_ runtime_missing_scene 1 'cannot open scene' \
  "$bin/sovereign-runtime" --scene "$work/does-not-exist.scene.json" --frames 5
printf '{ "schema_version": 1, "scene_id": ' > "$work/malformed.scene.json"
case_ runtime_malformed_scene 1 'cannot open scene.*(parse|JSON|json)' \
  "$bin/sovereign-runtime" --scene "$work/malformed.scene.json" --frames 5
case_ editor_missing_scene 1 'cannot open' \
  "$bin/sovereign-editor" --scene "$work/does-not-exist.scene.json"

# --- display ------------------------------------------------------------------------------------
case_ runtime_no_display 4 'SDL_Init failed|video' \
  env -u DISPLAY -u WAYLAND_DISPLAY -u SDL_VIDEO_DRIVER "$bin/sovereign-runtime" --scene "$scene" --frames 5
mkdir -p "$work/empty-path"
case_ ctest_launcher_without_display_or_xvfb 77 'skipped \(not passed\)' \
  env -u DISPLAY -u WAYLAND_DISPLAY PATH="$work/empty-path" /bin/bash "$src/scripts/ctest_display_launcher.sh" /bin/true

# --- Vulkan prerequisites (window created, then instance/device creation fails) -----------------
case_ runtime_no_vulkan_driver 4 "Vulkan doesn't implement|vkCreateInstance|VK_ERROR_INCOMPATIBLE_DRIVER" \
  env VK_ICD_FILENAMES="$work/no-such-icd.json" VK_DRIVER_FILES="$work/no-such-icd.json" \
  "$bin/sovereign-runtime" --scene "$scene" --frames 5
case_ runtime_required_validation_layer_missing 4 'VK_LAYER_KHRONOS_validation is required' \
  env VK_LAYER_PATH="$work/no-layers" SOV_REQUIRE_VALIDATION=1 \
  "$bin/sovereign-runtime" --scene "$scene" --frames 5
case_ runtime_requested_device_absent 4 'does not match the requested device' \
  "$bin/sovereign-runtime" --scene "$scene" --frames 5 --device no-such-gpu
# G1-R15: with a physical GPU required, a CPU implementation is refused (blocked), never used.
if grep -q 'evidence class: software-rendered' "$out/runtime_normal_close.log"; then
  case_ runtime_physical_gpu_required_on_cpu_only_host 4 'CPU/software implementation refused' \
    env SOV_REQUIRE_PHYSICAL_GPU=1 "$bin/sovereign-runtime" --scene "$scene" --frames 5
else
  case_ runtime_physical_gpu_required_on_gpu_host 0 'evidence class: gpu-rendered \(physical GPU\)' \
    env SOV_REQUIRE_PHYSICAL_GPU=1 "$bin/sovereign-runtime" --scene "$scene" --frames 5
fi

# --- partial initialization: device + swapchain exist, then shader loading fails -----------------
mkdir -p "$work/noshaders"
cp "$bin/sovereign-runtime" "$bin/sovereign-editor" "$work/noshaders/"
case_ runtime_missing_shaders_after_device_init 4 '\.spv' \
  "$work/noshaders/sovereign-runtime" --scene "$scene" --frames 5

# --- editor: failed init must not leave the scene locked or modified ---------------------------
cp "$scene" "$work/editor.scene.json"
before=$(sha256sum "$work/editor.scene.json" | cut -d' ' -f1)
case_ editor_requested_device_absent 4 'does not match the requested device' \
  "$bin/sovereign-editor" --scene "$work/editor.scene.json" --device no-such-gpu
case_ editor_missing_shaders_after_device_init 4 '\.spv' \
  "$work/noshaders/sovereign-editor" --scene "$work/editor.scene.json"
after=$(sha256sum "$work/editor.scene.json" | cut -d' ' -f1)
case_ editor_failed_init_left_scene_unchanged 0 "^$before\$" /bin/echo "$after"
# The writer lock is released: an offline CLI write is accepted afterwards (and only then).
cat > "$work/rename.command_batch.json" <<EOF
{"protocol_version":1,"request_id":"10000000-0000-4000-8000-00000000f001","actor":"cli:failure-paths",
 "scene_id":"$(python3 -c "import json,sys;print(json.load(open(sys.argv[1]))['scene_id'])" "$scene")",
 "base_revision":0,"idempotency_key":"failure-paths-after-editor","dry_run":false,
 "operations":[{"op":"entity.rename","entity_id":"$(python3 -c "import json,sys;print(json.load(open(sys.argv[1]))['entities'][1]['id'])" "$scene")","name":"Renamed after failed editor init"}]}
EOF
case_ cli_write_after_failed_editor_init 0 '"status": "committed"' \
  "$cli" scene apply --scene "$work/editor.scene.json" --commands "$work/rename.command_batch.json"

{
  echo '{'
  echo '  "tool": "scripts/failure_paths_check.sh",'
  echo "  \"commit\": \"$(git -C "$src" rev-parse HEAD 2>/dev/null || echo unknown)$(git -C "$src" diff --quiet HEAD 2>/dev/null || echo ' (modified)')\","
  echo "  \"failures\": $failures,"
  echo '  "cases": ['
  for i in "${!results[@]}"; do
    sep=','; [ "$i" -eq $(( ${#results[@]} - 1 )) ] && sep=''
    echo "    ${results[$i]}$sep"
  done
  echo '  ]'
  echo '}'
} > "$out/failure_paths.json"
echo "failure paths: $(( ${#results[@]} - failures ))/${#results[@]} passed; summary $out/failure_paths.json"
[ "$failures" -eq 0 ]
