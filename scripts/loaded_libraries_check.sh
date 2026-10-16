#!/usr/bin/env bash
# G0-06 run-time inspection: every shared object the runtime and editor actually load (including
# dlopen'ed ones: Vulkan loader, ICD, layers, X11) comes from the system library directories, and
# none belongs to an Unreal Engine installation.
#   scripts/loaded_libraries_check.sh <bin-dir> <out-dir>      (needs a display; see ctest_display_launcher.sh)
set -uo pipefail
bin="$(cd "$1" && pwd)"
out="$2"
mkdir -p "$out"
src="$(cd "$(dirname "$0")/.." && pwd)"
scene="$src/content/authoring/first_light.scene.json"
fail=0
check() { # check <name> <command...>
  local name="$1"; shift
  LD_DEBUG=files LD_DEBUG_OUTPUT="$out/$name.lddebug" "$@" > "$out/$name.log" 2>&1
  local code=$?
  # "file=<name> [0];  dynamically loaded by ..." marks dlopen'ed objects; "calling init: <path>"
  # gives the resolved path of every object initialised in the process.
  cat "$out/$name.lddebug".* 2>/dev/null | sed -n 's/^[[:space:]]*[0-9]*:[[:space:]]*file=\([^ ]*\) \[0\];[[:space:]]*dynamically loaded by.*/\1/p' |
    sort -u > "$out/$name.dlopened.txt"
  cat "$out/$name.lddebug".* 2>/dev/null | sed -n 's/^[[:space:]]*[0-9]*:[[:space:]]*calling init: \(.*\)$/\1/p' |
    sort -u > "$out/$name.loaded_paths.txt"
  rm -f "$out/$name.lddebug".*
  local n bad unreal
  n=$(wc -l < "$out/$name.loaded_paths.txt")
  unreal=$(grep -Ei 'unreal|epic|/Engine/Binaries|UE[45]' "$out/$name.loaded_paths.txt" || true)
  bad=$(grep -Ev '^(/lib|/usr/lib|/lib64|/usr/lib64)/' "$out/$name.loaded_paths.txt" || true)
  if [ "$code" -ne 0 ]; then
    echo "$name: exit $code (see $out/$name.log)"; fail=1
  elif [ "$n" -lt 5 ]; then
    echo "$name: only $n loaded objects recorded; LD_DEBUG capture failed"; fail=1
  elif [ -n "$unreal" ] || [ -n "$bad" ]; then
    echo "$name: libraries outside the system directories or from Unreal:"; echo "$unreal$bad" | sed 's/^/  /'; fail=1
  else
    echo "$name: $n shared objects loaded ($(wc -l < "$out/$name.dlopened.txt") via dlopen), all from system library directories, none from Unreal"
  fi
}
check runtime "$bin/sovereign-runtime" --scene "$scene" --frames 10
cp "$scene" "$out/editor.scene.json"
cat > "$out/quit.automation.json" <<'JSON'
{"steps": [{"do": "wait_frames", "n": 5}]}
JSON
check editor "$bin/sovereign-editor" --scene "$out/editor.scene.json" --automation "$out/quit.automation.json"
exit $fail
