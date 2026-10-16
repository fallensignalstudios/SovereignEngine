#!/usr/bin/env bash
# Collects the G0/G1 evidence set into docs/reports/evidence/ after both presets have been built and
# tested (`ctest --preset dev-vulkan` leaves its artifacts in build/dev-vulkan/test-artifacts).
#
#   1. offline rebuild of both presets inside a network namespace with no connectivity (G0-09)
#   2. ASan + UBSan headless build and tests (G0-16)
#   3. CLI demonstration (scripts/cli_demo_evidence.sh)
#   4. verbose dev-vulkan test run (validation summaries, device selection), then captures: runtime,
#      runtime on the editor-saved scene, editor automation, rendering tests, X server captures from
#      the real-input check, with a provenance index
#   5. gpu-qualify on this host, LAST (expected to refuse a CPU Vulkan device: G1-GPU-06 BLOCKED).
#      It re-runs the editor fixtures, which clear build/dev-vulkan/test-artifacts/editor.
#
# Usage: scripts/collect_evidence.sh            (Linux; needs unshare, Xvfb, openbox)
set -uo pipefail
cd "$(dirname "$0")/.."
E=docs/reports/evidence
L=$E/logs
mkdir -p "$L"
summary="$L/collect_summary.txt"; : > "$summary"
record() { echo "$1 exit $2" | tee -a "$summary"; }
logrun() {  # logrun <log-name> <cmd...>
  local name="$1"; shift
  { echo "\$ $*"; echo "# $(date -u +%Y-%m-%dT%H:%M:%SZ) commit $(git rev-parse HEAD)"; } > "$L/$name.log"
  "$@" >> "$L/$name.log" 2>&1; local code=$?
  echo "# exit $code at $(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$L/$name.log"
  record "$name" "$code"; return $code
}

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

# 3. CLI demonstration.
logrun cli_demo scripts/cli_demo_evidence.sh build/dev-headless/bin "$E/cli"

# 4. Fresh artifacts with verbose output (per-test notes: device, validation summary, resources).
logrun dev_vulkan_verbose ctest --preset dev-vulkan -V
A=build/dev-vulkan/test-artifacts
C=$E/captures
rm -rf "$C" "$E/editor" "$E/gui_real_input" && mkdir -p "$C/rendering" "$E/editor" "$E/gui_real_input"
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
for f in gui_real_input.log editor_stdout.log cli_while_editor_open.json cli_while_editor_open.stderr \
         history_after_gui.json diff_after_gui.txt x11_editor_after_pick.png x11_editor_after_delete.png \
         x11_editor_after_undo_save.png; do
  cp "$A/gui_real_input/$f" "$E/gui_real_input/" 2>/dev/null
done
python3 scripts/png_provenance.py "$C"/*.png "$C"/rendering/*.png > "$C/PROVENANCE.txt"
python3 scripts/png_provenance.py "$E"/gui_real_input/*.png >> "$C/PROVENANCE.txt"
sed -i "s#$(pwd)/##g" "$C/PROVENANCE.txt" "$E"/editor/*.json "$E"/gui_real_input/*.log "$E"/gui_real_input/*.json \
  "$E"/gui_real_input/*.txt "$E"/gui_real_input/*.stderr "$L"/*.log 2>/dev/null

# 5. GPU qualification on this host: must refuse llvmpipe. A non-zero exit is the expected result.
logrun gpu_qualify_on_this_host ctest --preset gpu-qualify --output-on-failure
grep -h 'physical device:' build/dev-vulkan/Testing/Temporary/LastTest.log | sort -u > "$L/gpu_qualify_device_selection.txt" || true
sed -i "s#$(pwd)/##g" "$L"/gpu_qualify*.log "$L"/gpu_qualify_device_selection.txt 2>/dev/null
echo "evidence collected; see $summary"
