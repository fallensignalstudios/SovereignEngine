#!/usr/bin/env bash
# Drives sovereign-editor with REAL X11 input events (xdotool) and verifies the outcome from
# outside the process. Run inside a display (scripts/with_virtual_display.sh on headless hosts).
#
#   1. start the editor on a copy of First Light (it takes the scene writer lock)
#   2. sovereign-cli tries an offline commit while the editor runs  -> must exit 3 (lock_unavailable)
#   3. mouse click on the Altar in the viewport (GPU pick)           -> selection
#   4. Delete key, Ctrl+Z, Ctrl+S                                   -> delete, undo, save
#   5. X server framebuffer capture of the editor window (independent of the engine's readback)
#   6. close the window; verify with sovereign-cli: journal holds [edit(delete Altar), undo],
#      revision 3, and the authored content equals the original (revision aside)
#
# Usage: scripts/gui_real_input_check.sh <build-bin-dir> <output-dir>
set -euo pipefail
BIN="$(cd "$1" && pwd)"
OUT="$2"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
CLI_HEADLESS="${SOV_CLI:-$REPO/build/dev-headless/bin/sovereign-cli}"
work="$OUT/work"
rm -rf "$work" && mkdir -p "$work"
cp "$REPO/content/authoring/first_light.scene.json" "$work/"
cp "$REPO/content/authoring/first_light.scene.json" "$work/original.scene.json"
cd "$work"

log() { echo "[gui-real-input] $*" | tee -a "$OUT/gui_real_input.log"; }
: > "$OUT/gui_real_input.log"

"$BIN/sovereign-editor" --scene first_light.scene.json --width 1600 --height 900 > "$OUT/editor_stdout.log" 2>&1 &
editor_pid=$!
trap 'kill $editor_pid 2>/dev/null || true' EXIT
wid=$(xdotool search --sync --onlyvisible --name "Sovereign Editor" | head -1)
log "editor pid $editor_pid window $wid"
for _ in $(seq 1 100); do grep -q 'viewport image at window' "$OUT/editor_stdout.log" && break; sleep 0.2; done
sleep 2  # let the layout settle at its final size
read -r ox oy vw vh < <(grep 'viewport image at window' "$OUT/editor_stdout.log" | tail -1 |
  sed -E 's/.*window \(([0-9.]+), ([0-9.]+)\) size ([0-9]+)x([0-9]+).*/\1 \2 \3 \4/')
log "viewport origin ($ox, $oy) size ${vw}x${vh}"

# 2. Cross-process writer lock: the CLI must refuse an offline commit while the editor holds the scene.
python3 - "$REPO/examples/place_banner.command_batch.json" "$work/commit.json" <<'PY'
import json, sys
b = json.load(open(sys.argv[1])); b["dry_run"] = False; b["idempotency_key"] = "gui-lock-check-0001"
b["request_id"] = "0b1e0000-0000-4000-8000-00000000d001"; json.dump(b, open(sys.argv[2], "w"), indent=2)
PY
set +e
"$CLI_HEADLESS" scene apply --scene first_light.scene.json --commands commit.json > "$OUT/cli_while_editor_open.json" 2> "$OUT/cli_while_editor_open.stderr"
cli_rc=$?
set -e
log "sovereign-cli commit while editor open: exit $cli_rc (expected 3)"
[ "$cli_rc" -eq 3 ] || { log "FAIL: expected lock_unavailable"; exit 1; }

# 3. Real mouse click on the Altar. Its projected centre in a 955x568 view from the scene camera
#    pose is (477, 267) (see the automation pick step); scale if the view size differs.
px=$(python3 -c "print(int($ox + 477 * $vw / 955))")
py=$(python3 -c "print(int($oy + 267 * $vh / 568))")
# XTEST injection (no --window): events take the X server's real input path, so SDL's XInput2
# handling sees them exactly like physical device input. (--window would send synthetic
# XSendEvent events, which XInput2 clients ignore.)
xdotool windowactivate --sync "$wid" 2>/dev/null || xdotool windowfocus --sync "$wid"
xdotool mousemove --window "$wid" "$px" "$py"
sleep 0.3
xdotool click 1
log "clicked viewport at window ($px, $py)"
sleep 1.5
import -window "$wid" "$OUT/x11_editor_after_pick.png" 2>/dev/null || import -window root "$OUT/x11_editor_after_pick.png"

# 4. Keyboard: Delete (delete selection), Ctrl+Z (undo), Ctrl+S (save).
xdotool mousemove --window "$wid" "$px" "$py"
xdotool key Delete; sleep 1
import -window "$wid" "$OUT/x11_editor_after_delete.png" 2>/dev/null || true
xdotool key ctrl+z; sleep 1
xdotool key ctrl+s; sleep 1.5
import -window "$wid" "$OUT/x11_editor_after_undo_save.png" 2>/dev/null || true
log "sent Delete, ctrl+z, ctrl+s"

# 6. Close through the window manager and verify from outside.
# Polite close: openbox maps Alt+F4 to WM_DELETE_WINDOW (SDL_EVENT_WINDOW_CLOSE_REQUESTED).
xdotool key alt+F4
for _ in $(seq 1 100); do kill -0 $editor_pid 2>/dev/null || break; sleep 0.1; done
set +e; wait $editor_pid; editor_rc=$?; set -e
trap - EXIT
log "editor exit code $editor_rc"

"$CLI_HEADLESS" scene history --scene first_light.scene.json --json > "$OUT/history_after_gui.json"
python3 - "$OUT/history_after_gui.json" "$work/original.scene.json" "$work/first_light.scene.json" <<'PY' | tee -a "$OUT/gui_real_input.log"
import json, sys
h = json.load(open(sys.argv[1]))
kinds = [t["kind"] for t in h["transactions"]]
orig, now = json.load(open(sys.argv[2])), json.load(open(sys.argv[3]))
altar = [e for e in orig["entities"] if e["name"] == "Altar"][0]
deleted_first = h["transactions"] and h["transactions"][0]["kind"] == "edit"
o2, n2 = dict(orig), dict(now); o2.pop("revision"); n2.pop("revision")
ok = kinds == ["edit", "undo"] and h["revision"] == 3 and o2 == n2 and any(e["id"] == altar["id"] for e in now["entities"])
print(f"[gui-real-input] journal kinds={kinds} revision={h['revision']} content_equals_original={o2 == n2} -> {'PASS' if ok else 'FAIL'}")
sys.exit(0 if ok else 1)
PY
"$CLI_HEADLESS" scene diff --before original.scene.json --after first_light.scene.json > "$OUT/diff_after_gui.txt"
[ "$editor_rc" -eq 0 ] || { log "FAIL: editor exited $editor_rc"; exit 1; }
grep -q 'validation: layer on, synchronization on, 0 error(s)' "$OUT/editor_stdout.log" || { log "FAIL: validation errors"; exit 1; }
log "PASS"
