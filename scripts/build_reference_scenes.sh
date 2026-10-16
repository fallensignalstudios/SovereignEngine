#!/usr/bin/env bash
# Builds the reference scene documents with the engine CLI:
#   content/authoring/first_light.scene.json  <- scene import of the owner's packet fixture
#                                                examples/first_light.scene.json (revision 0)
#   tests/fixtures/axis_lab.scene.json        <- scene new + scene apply of the generated batch
# Usage: scripts/build_reference_scenes.sh [path/to/sovereign-cli]
# Re-running is refused if a scene exists (new/import never overwrite); delete it first.
set -euo pipefail
cd "$(dirname "$0")/.."
CLI="${1:-build/dev-headless/bin/sovereign-cli}"

"$CLI" scene import --from examples/first_light.scene.json --scene content/authoring/first_light.scene.json
"$CLI" scene validate --scene content/authoring/first_light.scene.json

ids=$(python3 scripts/generate_scene_batches.py | sed -n 's/^ids: //p')
get() { python3 -c "import json,sys; print(json.loads(sys.argv[1])[sys.argv[2]])" "$ids" "$1"; }
"$CLI" scene new --scene tests/fixtures/axis_lab.scene.json --name "Axis Lab" \
  --scene-id "$(get axis_lab_scene)" --camera-id "$(get axis_lab_camera)" --background 0,0,0
"$CLI" scene apply --scene tests/fixtures/axis_lab.scene.json --commands tests/fixtures/axis_lab.build.command_batch.json > /dev/null
"$CLI" scene validate --scene tests/fixtures/axis_lab.scene.json
