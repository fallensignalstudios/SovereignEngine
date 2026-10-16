#!/usr/bin/env bash
# Builds the reference scene documents from their command batches using the engine CLI.
# Usage: scripts/build_reference_scenes.sh [path/to/sovereign-cli]
# Re-running is refused if the scenes exist (scene new never overwrites); delete them first.
set -euo pipefail
cd "$(dirname "$0")/.."
CLI="${1:-build/dev-headless/bin/sovereign-cli}"

ids=$(python3 scripts/generate_scene_batches.py | sed -n 's/^ids: //p')
get() { python3 -c "import json,sys; print(json.loads(sys.argv[1])[sys.argv[2]])" "$ids" "$1"; }

build() {  # scene_path name scene_id camera_id env batch
  "$CLI" scene new --scene "$1" --name "$2" --scene-id "$3" --camera-id "$4" --environment "$5"
  "$CLI" scene apply --scene "$1" --commands "$6" > /dev/null
  "$CLI" scene validate --scene "$1"
}

build content/authoring/first_light.scene.json "First Light" "$(get first_light_scene)" "$(get first_light_camera)" \
  content/source/first_light.environment.json content/source/first_light.build.command_batch.json
build tests/fixtures/axis_lab.scene.json "Axis Lab" "$(get axis_lab_scene)" "$(get axis_lab_camera)" \
  tests/fixtures/axis_lab.environment.json tests/fixtures/axis_lab.build.command_batch.json
