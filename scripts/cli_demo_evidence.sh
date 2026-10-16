#!/usr/bin/env bash
# G1 CLI demonstration (handoff "Expected CLI behavior"). Writes every command, exit code, stdout and
# stderr to <out>/transcript.md plus the JSON results, scene snapshots and journal.
#
# Usage: scripts/cli_demo_evidence.sh <bin-dir> <out-dir>
#
# The dry run runs on the canonical content/authoring/first_light.scene.json and proves it unchanged.
# Committing requests run on a copy (<out>/work/) so the repository's First Light stays at the
# revision the example batch targets; the CLI code path is identical.
set -uo pipefail
cd "$(dirname "$0")/.."
BIN="$(cd "$1" && pwd)"; OUT="$2"
CLI="$BIN/sovereign-cli"
SCENE=content/authoring/first_light.scene.json
EXAMPLE=examples/place_banner.command_batch.json
rm -rf "$OUT"; mkdir -p "$OUT/work" "$OUT/requests" "$OUT/results" "$OUT/scenes"
T="$OUT/transcript.md"
WORK="$OUT/work/first_light.scene.json"

step() {  # step <title> <expected-exit> <command...>; stdout -> $STDOUT_FILE if set
  local title="$1" want="$2"; shift 2
  local out err code
  out=$(mktemp); err=$(mktemp)
  "$@" >"$out" 2>"$err"; code=$?
  {
    echo "### $title"; echo; echo '```'; echo "\$ ${*/#$BIN\//}"; echo "exit $code (expected $want)"; echo '```'
    if [ -s "$out" ]; then echo; echo 'stdout:'; echo '```'; head -c 6000 "$out"; echo; echo '```'; fi
    if [ -s "$err" ]; then echo; echo 'stderr:'; echo '```'; head -c 3000 "$err"; echo '```'; fi
    echo
  } >>"$T"
  if [ -n "${STDOUT_FILE:-}" ]; then cp "$out" "$STDOUT_FILE"; fi
  rm -f "$out" "$err"
  if [ "$code" != "$want" ]; then echo "UNEXPECTED: $title exit $code, expected $want" | tee -a "$T"; FAILURES=$((FAILURES+1)); fi
  STDOUT_FILE=
}
sha() { sha256sum "$1" | cut -d' ' -f1; }
jget() { python3 -I -c "import json,sys; d=json.load(open(sys.argv[1])); print(eval(sys.argv[2], {'d': d}))" "$@"; }
note() { echo "$*" >>"$T"; echo >>"$T"; }
check() {  # check <description> <python-boolean-expression over shell-substituted values>
  if python3 -I -c "import sys; sys.exit(0 if ($2) else 1)"; then note "- [x] $1"; else note "- [ ] **FAILED:** $1"; FAILURES=$((FAILURES+1)); fi
}
FAILURES=0

echo "# CLI demonstration ($(date -u +%Y-%m-%dT%H:%M:%SZ))" >"$T"
note "Binaries: \`$(basename "$BIN")\` build of commit $(git rev-parse HEAD) ($(git diff --quiet HEAD && echo clean || echo modified)). Paths below are repository-relative."

note "## 1. Version and validation (canonical scene)"
STDOUT_FILE="$OUT/results/01_version.txt" step "sovereign-cli --version" 0 "$CLI" --version
STDOUT_FILE="$OUT/results/02_validate.txt" step "scene validate" 0 "$CLI" scene validate --scene "$SCENE"
STDOUT_FILE="$OUT/results/02_validate.json" step "scene validate --json" 0 "$CLI" scene validate --scene "$SCENE" --json

note "## 2. The example batch is a dry run and changes nothing"
before=$(sha "$SCENE"); ls -a content/authoring >"$OUT/results/03_dir_before.txt"
cp "$EXAMPLE" "$OUT/requests/place_banner.command_batch.json"
STDOUT_FILE="$OUT/results/03_example_dry_run.result.json" step "scene apply (example batch as shipped)" 0 \
  "$CLI" scene apply --scene "$SCENE" --commands "$EXAMPLE"
after=$(sha "$SCENE"); ls -a content/authoring >"$OUT/results/03_dir_after.txt"
note "scene sha256 before \`$before\`, after \`$after\`"
R="$OUT/results/03_example_dry_run.result.json"
check "status is dry_run" "'$(jget "$R" "d['status']")' == 'dry_run'"
check "revision_after == revision_before == 1" "$(jget "$R" "d['revision_after']") == $(jget "$R" "d['revision_before']") == 1"
check "scene file byte-identical after the dry run" "'$before' == '$after'"
check "no lock/journal/backup file created by the dry run" "open('$OUT/results/03_dir_before.txt').read() == open('$OUT/results/03_dir_after.txt').read()"
check "dry run reports the diff it would make (2 created, 1 modified)" "$(jget "$R" "len(d['diff']['entities_created'])") == 2 and $(jget "$R" "len(d['diff']['entities_modified'])") == 1"
check "every operation is reported 'valid' (passed validation), none 'applied'" "'$(jget "$R" "','.join(sorted({o['status'] for o in d['operations']}))")' == 'valid'"

note "## 3. A new, explicitly committed request"
cp "$SCENE" "$WORK"; cp "$SCENE" "$OUT/scenes/first_light.rev1.before.scene.json"
python3 -I - "$EXAMPLE" "$OUT/requests" <<'EOF'
import json, sys, uuid, copy
src, out = sys.argv[1], sys.argv[2]
b = json.load(open(src))
commit = copy.deepcopy(b)
commit.update(request_id=str(uuid.uuid4()), idempotency_key="place-banner-commit-0001", dry_run=False)
json.dump(commit, open(f"{out}/place_banner.commit.command_batch.json", "w"), indent=2)
retry = copy.deepcopy(commit); retry["request_id"] = str(uuid.uuid4())
json.dump(retry, open(f"{out}/place_banner.commit.retry_new_request_id.command_batch.json", "w"), indent=2)
changed = copy.deepcopy(commit); changed["request_id"] = str(uuid.uuid4())
for op in changed["operations"]:
    if op["op"] == "material.set":
        op["material"]["emissive_strength"] = 9.0
json.dump(changed, open(f"{out}/place_banner.changed_content_same_key.command_batch.json", "w"), indent=2)
stale = copy.deepcopy(commit); stale.update(request_id=str(uuid.uuid4()), idempotency_key="place-banner-stale-0001")
json.dump(stale, open(f"{out}/place_banner.stale_base_revision.command_batch.json", "w"), indent=2)
bad = copy.deepcopy(commit)
bad.update(request_id=str(uuid.uuid4()), idempotency_key="invalid-batch-0001", base_revision=2)
bad["operations"] = [{"op": "entity.rename", "entity_id": bad["operations"][0]["entity_id"], "name": "Renamed Banner"},
                     {"op": "entity.delete", "entity_id": "00000000-0000-4000-8000-0000000000ff"}]
json.dump(bad, open(f"{out}/invalid_second_op_unknown_entity.command_batch.json", "w"), indent=2)
EOF
REQ="$OUT/requests"
STDOUT_FILE="$OUT/results/04_commit.result.json" step "scene apply (dry_run: false)" 0 \
  "$CLI" scene apply --scene "$WORK" --commands "$REQ/place_banner.commit.command_batch.json"
R="$OUT/results/04_commit.result.json"
check "status committed, delivery original, revision 1 -> 2, every operation applied" "'$(jget "$R" "d['status']")' == 'committed' and '$(jget "$R" "d['delivery']")' == 'original' and $(jget "$R" "d['revision_after']") == 2 and '$(jget "$R" "','.join(sorted({o['status'] for o in d['operations']}))")' == 'applied'"
cp "$WORK" "$OUT/scenes/first_light.rev2.after_commit.scene.json"
STDOUT_FILE="$OUT/results/05_diff.json" step "scene diff before/after (structural)" 0 \
  "$CLI" scene diff --before "$OUT/scenes/first_light.rev1.before.scene.json" --after "$OUT/scenes/first_light.rev2.after_commit.scene.json" --json
diff -u "$OUT/scenes/first_light.rev1.before.scene.json" "$OUT/scenes/first_light.rev2.after_commit.scene.json" >"$OUT/results/05_text.diff"
note "Text diff of the scene file: \`results/05_text.diff\` ($(grep -c '^[+-][^+-]' "$OUT/results/05_text.diff") changed lines)."
STDOUT_FILE="$OUT/results/05_validate_after.txt" step "scene validate (after commit)" 0 "$CLI" scene validate --scene "$WORK"

note "## 4. Retry behaviour"
s2=$(sha "$WORK")
STDOUT_FILE="$OUT/results/06_retry_same.result.json" step "retry: identical request delivered again" 0 \
  "$CLI" scene apply --scene "$WORK" --commands "$REQ/place_banner.commit.command_batch.json"
R="$OUT/results/06_retry_same.result.json"
check "replayed: recorded result returned, delivery replayed, same commit id, revision still 2" \
  "'$(jget "$R" "d['delivery']")' == 'replayed' and '$(jget "$R" "d['commit_id']")' == '$(jget "$OUT/results/04_commit.result.json" "d['commit_id']")' and $(jget "$R" "d['revision_after']") == 2"
check "scene unchanged by the replay" "'$s2' == '$(sha "$WORK")'"
STDOUT_FILE="$OUT/results/07_retry_new_request_id.result.json" step "retry: same content, new request_id (transport field)" 0 \
  "$CLI" scene apply --scene "$WORK" --commands "$REQ/place_banner.commit.retry_new_request_id.command_batch.json"
check "still replayed; scene unchanged" "'$(jget "$OUT/results/07_retry_new_request_id.result.json" "d['delivery']")' == 'replayed' and '$s2' == '$(sha "$WORK")'"
STDOUT_FILE="$OUT/results/08_changed_content.result.json" step "same idempotency key, changed content" 1 \
  "$CLI" scene apply --scene "$WORK" --commands "$REQ/place_banner.changed_content_same_key.command_batch.json"
check "rejected with idempotency_conflict; scene unchanged" "'$(jget "$OUT/results/08_changed_content.result.json" "d['errors'][0]['code']")' == 'idempotency_conflict' and '$s2' == '$(sha "$WORK")'"

note "## 5. Stale revision and invalid batch"
STDOUT_FILE="$OUT/results/09_stale.result.json" step "new key, base_revision 1 while scene is at 2" 1 \
  "$CLI" scene apply --scene "$WORK" --commands "$REQ/place_banner.stale_base_revision.command_batch.json"
R="$OUT/results/09_stale.result.json"
check "stale_revision with conflict details (current 2, one newer commit)" "'$(jget "$R" "d['errors'][0]['code']")' == 'stale_revision' and $(jget "$R" "d['conflict']['current_revision']") == 2 and $(jget "$R" "len(d['conflict']['commits_since_base'])") == 1"
STDOUT_FILE="$OUT/results/10_invalid.result.json" step "valid op 0, invalid op 1 (unknown entity)" 1 \
  "$CLI" scene apply --scene "$WORK" --commands "$REQ/invalid_second_op_unknown_entity.command_batch.json"
R="$OUT/results/10_invalid.result.json"
check "whole batch rejected: op0 'valid' but not applied, op1 'failed', revision unchanged, file unchanged" \
  "'$(jget "$R" "d['status']")' == 'rejected' and '$(jget "$R" "d['operations'][0]['status']")' == 'valid' and '$(jget "$R" "d['operations'][1]['status']")' == 'failed' and $(jget "$R" "d['revision_after']") == 2 and '$s2' == '$(sha "$WORK")'"

note "## 6. Undo / redo (new revisions, content restored)"
STDOUT_FILE="$OUT/results/11_undo.result.json" step "scene undo --base-revision 2" 0 "$CLI" scene undo --scene "$WORK" --base-revision 2 --actor cli:demo
cp "$WORK" "$OUT/scenes/first_light.rev3.after_undo.scene.json"
STDOUT_FILE="$OUT/results/12_undo_inspect.json" step "inspect after undo" 0 "$CLI" scene inspect --scene "$WORK" --json
STDOUT_FILE="$OUT/results/13_redo.result.json" step "scene redo --base-revision 3" 0 "$CLI" scene redo --scene "$WORK" --base-revision 3 --actor cli:demo
cp "$WORK" "$OUT/scenes/first_light.rev4.after_redo.scene.json"
python3 -I - "$OUT/scenes" >"$OUT/results/14_content_hashes.txt" <<'EOF'
import json, sys, hashlib, glob, os
for p in sorted(glob.glob(sys.argv[1] + "/*.scene.json")):
    d = json.load(open(p)); rev = d.pop("revision")
    h = hashlib.sha256(json.dumps(d, sort_keys=True).encode()).hexdigest()
    print(f"{os.path.basename(p)} revision {rev} content-without-revision {h}")
EOF
cat "$OUT/results/14_content_hashes.txt" >>"$T"; echo >>"$T"
h() { grep "^$1 " "$OUT/results/14_content_hashes.txt" | awk '{print $5}'; }
check "undo (rev 3) content == rev 1 content; redo (rev 4) content == rev 2 content; revisions 1,2,3,4" \
  "'$(h first_light.rev3.after_undo.scene.json)' == '$(h first_light.rev1.before.scene.json)' and '$(h first_light.rev4.after_redo.scene.json)' == '$(h first_light.rev2.after_commit.scene.json)'"
STDOUT_FILE="$OUT/results/15_history.json" step "scene history --json" 0 "$CLI" scene history --scene "$WORK" --json
STDOUT_FILE="$OUT/results/16_stale_undo.result.json" step "undo with an outdated base revision" 1 "$CLI" scene undo --scene "$WORK" --base-revision 2

note "## 7. Writer lock: offline write refused while another writer holds the scene"
note "Here the lock is held by util-linux \`flock\` on the same lock file (same flock(2) lock the editor takes). The real-editor variant is \`vulkan.editor_real_x11_input\` (evidence/gui_real_input)."
s4=$(sha "$WORK")
flock "$WORK.lock" sleep 6 & HOLDER=$!; sleep 1
STDOUT_FILE="$OUT/results/17_locked.result.json" step "committing apply while lock is held" 3 \
  "$CLI" scene apply --scene "$WORK" --commands "$REQ/place_banner.stale_base_revision.command_batch.json"
STDOUT_FILE="$OUT/results/18_locked_read.txt" step "read-only validate while lock is held" 0 "$CLI" scene validate --scene "$WORK"
wait $HOLDER
check "scene unchanged while locked" "'$s4' == '$(sha "$WORK")'"

cp "$WORK.journal" "$OUT/scenes/first_light.journal.jsonl"
note "Journal copied to \`scenes/first_light.journal.jsonl\` ($(wc -l <"$WORK.journal") lines)."
rm -rf "$OUT/work"
note "## Summary"
note "Unexpected exit codes or failed checks: **$FAILURES**"
sed -i "s#$(pwd)/##g" "$T" "$OUT"/results/* 2>/dev/null || true
echo "CLI demo: $FAILURES failure(s); transcript $T"
exit $((FAILURES > 0))
