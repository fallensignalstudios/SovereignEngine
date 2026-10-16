#!/usr/bin/env bash
# Collects the G0/G1 evidence set into docs/reports/evidence/ after both presets have been built and
# tested (`ctest --preset dev-vulkan` leaves its artifacts in build/dev-vulkan/test-artifacts).
# Matrix IDs are those of docs/VALIDATION_MATRIX.md (the owner's packet).
#
#   0. isolation probe, before (G0-01)
#   1. offline rebuild of both presets inside a network namespace with no connectivity (G0-03, G0-04)
#   2. ASan + UBSan headless build and tests (G0-05 and the G1-D checks under sanitizers)
#   3. CLI demonstration on the packet example (scripts/cli_demo_evidence.sh; G1-D05..D11, D14)
#   4. verbose dev-vulkan test run, then: captures with a provenance index (G1-R12), editor
#      automation (G1-R06, R07, R11), real-input GUI session (G1-R09, D14, D16), runtime on the
#      editor-saved scene (G1-R08)
#   5. failure paths: run time (G1-R14, G0-07, from the CTest run) and configure time (G0-07)
#   6. dependency inspection: link-time NEEDED and run-time loaded objects (G0-06)
#   7. performance reports from the runtime (G1-R13 procedure; software-rendered here)
#   8. gpu-qualify on this host, LAST (expected to refuse a CPU Vulkan device: G1-R01/R15).
#      It re-runs the editor fixtures, which clear build/dev-vulkan/test-artifacts/editor.
#   9. isolation probe, after, and the before/after comparison (G0-01)
#
# Usage: scripts/collect_evidence.sh            (Linux; needs unshare, Xvfb, openbox, python3)
set -uo pipefail
cd "$(dirname "$0")/.."
E=docs/reports/evidence
L=$E/logs
mkdir -p "$L" "$E/isolation"
summary="$L/collect_summary.txt"; : > "$summary"
record() { echo "$1 exit $2" | tee -a "$summary"; }
logrun() {  # logrun <log-name> <cmd...>
  local name="$1"; shift
  { echo "\$ $*"; echo "# $(date -u +%Y-%m-%dT%H:%M:%SZ) commit $(git rev-parse HEAD)"; } > "$L/$name.log"
  "$@" >> "$L/$name.log" 2>&1; local code=$?
  echo "# exit $code at $(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$L/$name.log"
  record "$name" "$code"; return $code
}
scrub() {  # repository-relative paths; the session scratch directory is not a public path
  sed -i -e "s#$(pwd)/##g" -e "s#/tmp/claude-[^ /]*/[^ ]*scratchpad#<session-scratch>#g" "$@" 2>/dev/null || true
}

# 0. Isolation, before.
scripts/isolation_check.sh "$E/isolation/before.txt" "before evidence collection"

# 1. Offline rebuild. A touched source file forces recompilation and a reconfigure re-reads the
#    lock; FetchContent must use the populated build/<preset>/_deps without network.
offline() {
  echo "network check inside the namespace (must fail):"
  curl -sS -m 5 -o /dev/null https://github.com && { echo "NETWORK REACHABLE - not an offline test"; return 1; }
  git ls-remote https://github.com/libsdl-org/SDL >/dev/null 2>&1 && { echo "git reached network"; return 1; }
  echo "-> no network"
  touch engine/core/src/build_info.cpp engine/renderer/src/renderer.cpp
  cmake --preset dev-headless && cmake --build --preset dev-headless && ctest --preset dev-headless || return 1
  cmake --preset dev-vulkan && cmake --build --preset dev-vulkan &&
    ctest --preset dev-vulkan -R 'static\.|vulkan\.rendering' --output-on-failure
}
logrun offline_rebuild_both_presets unshare -rn bash -c "$(declare -f offline); offline"

# 2. Sanitizers (fresh tree; fetches only headless dependencies).
rm -rf build/dev-headless-asan
logrun asan_1_configure cmake --preset dev-headless-asan &&
  logrun asan_2_build cmake --build --preset dev-headless-asan &&
  logrun asan_3_test ctest --preset dev-headless-asan --output-on-failure

# 3. CLI demonstration (headless build: no graphics code involved).
logrun cli_demo scripts/cli_demo_evidence.sh build/dev-headless/bin "$E/cli"

# 4. Fresh artifacts with verbose output (per-test notes: device, validation summary, resources).
logrun dev_vulkan_verbose ctest --preset dev-vulkan -V
A=build/dev-vulkan/test-artifacts
C=$E/captures
rm -rf "$C" "$E/editor" "$E/gui_real_input" "$E/failure_paths" "$E/dependencies" "$E/performance"
mkdir -p "$C/rendering" "$E/editor" "$E/gui_real_input" "$E/failure_paths/runtime" "$E/failure_paths/configure" \
  "$E/dependencies" "$E/performance"
cp "$A/runtime_first_light.png" "$C/runtime_first_light.png"
cp "$A"/editor/*.png "$C/" 2>/dev/null
cp "$A/editor/editor_report.json" "$E/editor/editor_report.json"
cp content/authoring/first_light.scene.json "$E/editor/first_light.before_editor.scene.json"
cp "$A/editor/first_light.scene.json" "$E/editor/first_light.saved_by_editor.scene.json"
cp "$A/editor/first_light.scene.json.journal" "$E/editor/first_light.saved_by_editor.journal.jsonl" 2>/dev/null
build/dev-headless/bin/sovereign-cli scene diff --before "$E/editor/first_light.before_editor.scene.json" \
  --after "$E/editor/first_light.saved_by_editor.scene.json" --json > "$E/editor/editor_session.diff.json"
build/dev-headless/bin/sovereign-cli scene history --scene "$A/editor/first_light.scene.json" --json > "$E/editor/editor_session.history.json"
# The separate runtime opens the scene the editor saved (no editor code: static.runtime_has_no_editor_code).
logrun runtime_opens_editor_saved_scene scripts/with_virtual_display.sh build/dev-vulkan/bin/sovereign-runtime \
  --scene "$A/editor/first_light.scene.json" --width 1280 --height 720 --frames 90 \
  --screenshot "$C/runtime_editor_saved_scene.png"
find build/dev-vulkan/test-tmp -path '*/rendering.*' -name '*.png' -exec cp {} "$C/rendering/" \;
find "$A/gui_real_input" -maxdepth 1 -type f -exec cp {} "$E/gui_real_input/" \;
python3 -I scripts/check_contracts.py "$E/editor" "$E/gui_real_input" > "$E/editor/owner_schema_check.txt" 2>&1
record owner_schema_check_of_editor_written_scenes $?
python3 -I scripts/png_provenance.py "$C"/*.png "$C"/rendering/*.png > "$C/PROVENANCE.txt"
python3 -I scripts/png_provenance.py "$E"/gui_real_input/*.png >> "$C/PROVENANCE.txt"

# 5. Failure paths. Run time: the CTest run above (vulkan.failure_paths_unwind_cleanly).
find "$A/failure_paths" -maxdepth 1 -type f -exec cp {} "$E/failure_paths/runtime/" \;
logrun configure_failure_paths scripts/configure_failure_paths.sh "$E/failure_paths/configure"

# 6. Dependency inspection.
{
  for b in build/dev-vulkan/bin/sovereign-runtime build/dev-vulkan/bin/sovereign-editor build/dev-vulkan/bin/sovereign-cli \
           build/dev-headless/bin/sovereign-cli; do
    echo "== $b"; readelf -d "$b" | grep -E 'NEEDED|RPATH|RUNPATH'
  done
} > "$E/dependencies/link_time_needed.txt"
find "$A/loaded_libraries" -maxdepth 1 -name '*.txt' -exec cp {} "$E/dependencies/" \;
cp "$A/loaded_libraries/runtime.log" "$E/dependencies/runtime_loaded_libraries_run.log" 2>/dev/null

# 7. Performance reports (R13 procedure). On this host they are CPU-Vulkan numbers, not a baseline.
perf() {  # perf <name> <runtime args...>
  local name="$1"; shift
  logrun "perf_$name" scripts/with_virtual_display.sh build/dev-vulkan/bin/sovereign-runtime \
    --scene content/authoring/first_light.scene.json --frames 720 --warmup 120 --perf-report "$E/performance/$name.json" "$@"
}
perf first_light_1280x720_vsync --width 1280 --height 720
perf first_light_1280x720_novsync --width 1280 --height 720 --no-vsync
perf first_light_1920x1080_novsync --width 1920 --height 1080 --no-vsync
perf first_light_1920x1080_novsync_novalidation --width 1920 --height 1080 --no-vsync --no-validation

# 8. GPU qualification on this host: must refuse llvmpipe. A non-zero exit is the expected result.
logrun gpu_qualify_on_this_host ctest --preset gpu-qualify --output-on-failure
grep -h 'physical device:' build/dev-vulkan/Testing/Temporary/LastTest.log | sort -u > "$L/gpu_qualify_device_selection.txt" || true

# 9. Isolation, after.
scripts/isolation_check.sh "$E/isolation/after.txt" "after evidence collection"
diff <(sed '1,2d' "$E/isolation/before.txt") <(sed '1,2d' "$E/isolation/after.txt") > "$E/isolation/before_after.diff"
record isolation_before_after_identical $?

scrub "$C/PROVENANCE.txt" "$E"/editor/*.json "$E"/editor/*.txt "$E"/gui_real_input/*.log "$E"/gui_real_input/*.json \
  "$E"/gui_real_input/*.txt "$E"/gui_real_input/*.stderr "$E"/failure_paths/*/* "$E"/dependencies/* \
  "$E"/performance/*.json "$E"/isolation/* "$L"/*.log "$L"/*.txt
echo "evidence collected; see $summary"
