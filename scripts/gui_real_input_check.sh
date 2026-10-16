#!/usr/bin/env bash
# Drives sovereign-editor with REAL X11 input events (xdotool XTEST: the X server's real input path,
# which SDL's XInput2 handling treats exactly like physical devices) and verifies every outcome from
# outside the process. Widget positions come from the editor's opt-in UI-anchor file
# (SOV_UI_ANCHORS); the anchors only say where widgets are, the actions are real input.
#
#   1. editor opens First Light (takes the writer lock); a CLI commit is refused (exit 3)
#   2. hierarchy click selects "Column 2"; Ctrl+click its Position X, type -5.5, Enter (transform.set)
#   3. hierarchy click "Platform light strip"; Ctrl+click Emissive R, type 3, Enter (material.set)
#   4. viewport click picks "Raised platform"; Delete key deletes it
#   5. History panel: Undo button, Redo button, then Ctrl+Z
#   6. Diagnostics: dock tab click, Validation tab (0 errors shown), Commands tab
#   7. Ctrl+S; X server captures along the way; Alt+F4 closes the window
#   8. outside checks: journal [edit, edit, edit, undo, redo, undo] at revision 6, field values
#   9. the editor is launched again on the saved file: same revision, history and values (save-close-reopen)
#  10. sovereign-runtime opens the saved scene
#
# Usage: scripts/gui_real_input_check.sh <build-bin-dir> <output-dir>
set -euo pipefail
BIN="$(cd "$1" && pwd)"
OUT="$2"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
CLI="${SOV_CLI:-$REPO/build/dev-headless/bin/sovereign-cli}"
work="$OUT/work"
rm -rf "$work" && mkdir -p "$work"
rm -f "$OUT"/x11_*.png
cp "$REPO/content/authoring/first_light.scene.json" "$work/"
cp "$REPO/content/authoring/first_light.scene.json" "$work/original.scene.json"
cd "$work"
ANCHORS="$work/anchors.json"
: > "$OUT/gui_real_input.log"
log() { echo "[gui-real-input] $*" | tee -a "$OUT/gui_real_input.log"; }
fail() { log "FAIL: $*"; exit 1; }

# aq EXPR: evaluate a Python expression over the anchors document `d` (prints the result).
aq() { python3 -I -c 'import json,sys
try: d=json.load(open(sys.argv[1]))
except Exception: d={}
try: print(eval(sys.argv[2], {"d": d}))
except Exception: print("")' "$ANCHORS" "$1"; }
# wait_for EXPR WHAT [seconds]: poll until EXPR is true in the anchors.
wait_for() {
  local expr="$1" what="$2" limit="${3:-15}"
  for _ in $(seq 1 $((limit * 10))); do
    [ "$(aq "$expr")" = "True" ] && { log "ok: $what"; return 0; }
    kill -0 "$editor_pid" 2>/dev/null || fail "editor exited while waiting for: $what"
    sleep 0.1
  done
  fail "timed out waiting for: $what (state: $(aq "d.get('state')"))"
}
# centre GROUP KEY -> "x y" (window pixels)
centre() { aq "' '.join(str(int(v)) for v in ((d['$1']['$2'][0]+d['$1']['$2'][2])/2, (d['$1']['$2'][1]+d['$1']['$2'][3])/2))"; }
point() { aq "' '.join(str(int(v)) for v in d['$1']['$2'])"; }
# Clicks hold the button for 80 ms like a physical mouse. A zero-length synthetic click (xdotool
# click) reaches SDL 3.2 twice: SDL reads XInput2 button events from both the master and the slave
# device and relies on button state to drop the copy, which only works when press and release are
# not back-to-back (see docs/reports/G0_G1_REPORT.md, defects).
press() { xdotool mousedown 1; sleep 0.08; xdotool mouseup 1; }
click_at() { xdotool mousemove --window "$wid" $1; sleep 0.15; press; sleep 0.25; }
ctrl_click_at() { xdotool mousemove --window "$wid" $1; sleep 0.15; xdotool keydown ctrl; press; xdotool keyup ctrl; sleep 0.3; }
type_value() { xdotool key ctrl+a; xdotool type --delay 40 -- "$1"; xdotool key Return; sleep 0.3; }
capture() { import -window "$wid" "$OUT/$1" 2>/dev/null || import -window root "$OUT/$1"; log "captured $1"; }

start_editor() {  # start_editor <log-name>
  rm -f "$ANCHORS"
  SOV_UI_ANCHORS="$ANCHORS" "$BIN/sovereign-editor" --scene first_light.scene.json --width 1600 --height 900 \
    > "$OUT/$1" 2>&1 &
  editor_pid=$!
  trap 'kill $editor_pid 2>/dev/null || true' EXIT
  # Fail fast if the editor exits before showing a window (e.g. no acceptable Vulkan device under
  # gpu-qualify); `xdotool search --sync` would wait forever.
  wid=""
  for _ in $(seq 1 300); do
    if ! kill -0 "$editor_pid" 2>/dev/null; then
      set +e; wait "$editor_pid"; local rc=$?; set -e
      trap - EXIT
      log "FAIL: editor exited with code $rc before showing a window (see $1)"
      tail -5 "$OUT/$1" | tee -a "$OUT/gui_real_input.log"
      exit 1
    fi
    wid=$(xdotool search --onlyvisible --name "Sovereign Editor" 2>/dev/null | head -1 || true)
    [ -n "$wid" ] && [ -s "$ANCHORS" ] && break
    sleep 0.2
  done
  [ -n "$wid" ] || fail "no editor window after 60 s"
  xdotool windowactivate --sync "$wid" 2>/dev/null || xdotool windowfocus --sync "$wid"
  sleep 1.5  # let the docked layout settle at its final size
  log "editor pid $editor_pid window $wid"
}
close_editor() {
  xdotool windowactivate --sync "$wid" 2>/dev/null || true
  xdotool key alt+F4  # openbox maps Alt+F4 to WM_DELETE_WINDOW (SDL_EVENT_WINDOW_CLOSE_REQUESTED)
  for _ in $(seq 1 100); do kill -0 "$editor_pid" 2>/dev/null || break; sleep 0.1; done
  set +e; wait "$editor_pid"; local rc=$?; set -e
  trap - EXIT
  log "editor exit code $rc"
  [ "$rc" -eq 0 ] || fail "editor exited $rc"
}

# 1. Open, and refuse an offline write while the editor holds the scene.
start_editor editor_stdout.log
wait_for "d['state']['revision'] == 0 and not d['state']['dirty']" "scene open at revision 0"
python3 -I - "$REPO/examples/place_banner.command_batch.json" "$work/commit.json" <<'PY'
import json, sys
b = json.load(open(sys.argv[1])); b["dry_run"] = False; b["idempotency_key"] = "gui-lock-check-0001"
b["request_id"] = "0b1e0000-0000-4000-8000-00000000d001"; json.dump(b, open(sys.argv[2], "w"), indent=2)
PY
set +e
"$CLI" scene apply --scene first_light.scene.json --commands commit.json > "$OUT/cli_while_editor_open.json" 2> "$OUT/cli_while_editor_open.stderr"
cli_rc=$?
set -e
log "sovereign-cli commit while editor open: exit $cli_rc (expected 3)"
[ "$cli_rc" -eq 3 ] || fail "expected lock_unavailable (exit 3)"
capture x11_01_opened.png

# 2. Hierarchy selection + typed numeric inspector edit (transform.set).
click_at "$(centre hierarchy 'Column 2')"
wait_for "d['state']['selected']['name'] == 'Column 2'" "hierarchy click selected Column 2"
wait_for "'Position.x' in d.get('inspector', {})" "inspector shows Column 2"
ctrl_click_at "$(centre inspector Position.x)"
type_value "-5.5"
wait_for "d['state']['revision'] == 1 and d['state']['selected']['components']['Transform']['position'][0] == -5.5" \
  "typed Position X = -5.5 committed as revision 1"
capture x11_02_position_typed.png

# 3. Material change through the inspector (material.set).
click_at "$(centre hierarchy 'Platform light strip')"
wait_for "d['state']['selected']['name'] == 'Platform light strip' and 'Emissive.x' in d.get('inspector', {})" \
  "hierarchy click selected Platform light strip"
ctrl_click_at "$(centre inspector Emissive.x)"
type_value "3"
wait_for "d['state']['revision'] == 2 and d['state']['selected']['components']['MaterialParameters']['emissive_rgb'][0] == 3.0" \
  "typed Emissive R = 3 committed as revision 2"
capture x11_03_material_typed.png

# 4. Viewport pick (GPU ID readback) + Delete key.
click_at "$(point viewport_entities 'Raised platform')"
wait_for "d['state']['selected'] and d['state']['selected']['name'] == 'Raised platform'" "viewport click picked Raised platform"
capture x11_04_viewport_pick.png
xdotool key Delete; sleep 0.3
wait_for "d['state']['revision'] == 3 and d['state']['entity_count'] == 11 and d['state']['selected'] is None" \
  "Delete key removed Raised platform (revision 3)"

# 5. History panel buttons, then the keyboard shortcut.
click_at "$(centre dock_tabs History)"
wait_for "d['state'].get('History_shown') is True" "History panel showing"
click_at "$(centre buttons Undo)"
wait_for "d['state']['revision'] == 4 and d['state']['entity_count'] == 12" "Undo button restored the platform (revision 4)"
click_at "$(centre buttons Redo)"
wait_for "d['state']['revision'] == 5 and d['state']['entity_count'] == 11" "Redo button deleted it again (revision 5)"
xdotool key ctrl+z; sleep 0.3
wait_for "d['state']['revision'] == 6 and d['state']['entity_count'] == 12" "Ctrl+Z restored it (revision 6)"
capture x11_05_history.png

# 6. Diagnostics panel visibly usable.
click_at "$(centre dock_tabs Diagnostics)"
wait_for "d['state'].get('Diagnostics_shown') is True and 'Validation' in d.get('diagnostics_tabs', {})" "Diagnostics panel showing"
click_at "$(centre diagnostics_tabs Validation)"
wait_for "d['state'].get('diagnostics_tab') == 'Validation' and d['state']['diagnostics_validation']['layer'] and d['state']['diagnostics_validation']['errors'] == 0" \
  "Validation tab shows layer on, 0 errors"
capture x11_06_diagnostics_validation.png
click_at "$(centre diagnostics_tabs Commands)"
wait_for "d['state'].get('diagnostics_tab') == 'Commands' and d['state']['diagnostics_command_results'] >= 6" \
  "Commands tab lists the session's command results"

# 7. Save and close.
xdotool key ctrl+s; sleep 0.5
wait_for "not d['state']['dirty']" "Ctrl+S saved (not dirty)"
capture x11_07_saved.png
close_editor
grep -q 'validation: layer on, synchronization on, 0 error(s)' "$OUT/editor_stdout.log" || fail "validation errors in first session"

# 8. Outside checks: journal and authored values.
"$CLI" scene history --scene first_light.scene.json --json > "$OUT/history_after_gui.json"
"$CLI" scene diff --before original.scene.json --after first_light.scene.json > "$OUT/diff_after_gui.txt"
cp first_light.scene.json "$OUT/first_light.saved_by_gui.scene.json"
python3 -I - "$OUT/history_after_gui.json" "$work/original.scene.json" "$work/first_light.scene.json" <<'PY' | tee -a "$OUT/gui_real_input.log"
import json, sys
h = json.load(open(sys.argv[1]))
orig, now = json.load(open(sys.argv[2])), json.load(open(sys.argv[3]))
kinds = [t["kind"] for t in h["transactions"]]
by_name = {e["name"]: e for e in now["entities"]}
col = by_name["Column 2"]["components"]["Transform"]["position"]
strip = by_name["Platform light strip"]["components"]["MaterialParameters"]["emissive_rgb"]
o_by = {e["name"]: e for e in orig["entities"]}
others_same = all(o_by[n] == by_name[n] for n in o_by if n not in ("Column 2", "Platform light strip"))
ok = (kinds == ["edit", "edit", "edit", "undo", "redo", "undo"] and h["revision"] == 6 and now["revision"] == 6
      and col[0] == -5.5 and col[1:] == o_by["Column 2"]["components"]["Transform"]["position"][1:]
      and strip == [3.0] + o_by["Platform light strip"]["components"]["MaterialParameters"]["emissive_rgb"][1:]
      and "Raised platform" in by_name and len(now["entities"]) == 12 and others_same)
print(f"[gui-real-input] journal kinds={kinds} revision={h['revision']} column2.x={col[0]} strip.emissive={strip} "
      f"platform_present={'Raised platform' in by_name} other_entities_unchanged={others_same} -> {'PASS' if ok else 'FAIL'}")
sys.exit(0 if ok else 1)
PY

# 9. Save-close-reopen: a new editor process shows the same authored state and history.
start_editor editor_reopen_stdout.log
wait_for "d['state']['revision'] == 6 and d['state']['history_size'] == 6 and not d['state']['dirty'] and d['state']['entity_count'] == 12" \
  "reopened editor: revision 6, 6 history records, clean"
click_at "$(centre hierarchy 'Column 2')"
wait_for "d['state']['selected']['name'] == 'Column 2' and d['state']['selected']['components']['Transform']['position'][0] == -5.5" \
  "reopened editor shows Column 2 at x = -5.5"
capture x11_08_reopened.png
close_editor

# 10. The standalone runtime opens the scene the GUI saved.
"$BIN/sovereign-runtime" --scene first_light.scene.json --width 1280 --height 720 --frames 60 \
  --screenshot "$OUT/runtime_after_gui_session.png" > "$OUT/runtime_after_gui_session.log" 2>&1 || fail "runtime could not open the saved scene"
log "runtime opened the GUI-saved scene and presented frames"
log "PASS"
