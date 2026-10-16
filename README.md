# Sovereign Engine (research build, First Ignition)

A separate, independent research engine: native editor, renderer, runtime and tools in C++20 on a
single Vulkan 1.3 backend. It is not an Unreal plugin and depends on no Unreal code. Private research;
no licence has been chosen (ADR-009).

Status: **G0 + G1 implemented; GPU qualification blocked** on the build host (no physical GPU).
Read `docs/reports/G0_G1_REPORT.md` for exactly what was verified and how.

## Quick start

```
# Headless: core, scene, commands, CLI, tests. No display, SDL or Vulkan needed.
cmake --preset dev-headless
cmake --build --preset dev-headless
ctest --preset dev-headless

# Graphics: editor, runtime, Vulkan rendering tests.
scripts/build_dxc_from_source.sh          # Linux, once per host (ADR-015)
cmake --preset dev-vulkan
cmake --build --preset dev-vulkan
ctest --preset dev-vulkan

# On a machine with a physical GPU: refuses software Vulkan devices.
ctest --preset gpu-qualify
```

Prerequisites: CMake >= 3.25, Ninja, GCC 13+ / Clang 17+ / MSVC 19.38+, git, Python 3; for
`dev-vulkan` a Vulkan 1.3 driver, the Vulkan loader, `VK_LAYER_KHRONOS_validation`, and on Linux the
X11 development headers listed in `dependencies.lock.json`. All third-party code is fetched by CMake at
the pinned revisions in `dependencies.lock.json`; afterwards builds work offline.

## Try it

```
B=build/dev-headless/bin
$B/sovereign-cli --version
$B/sovereign-cli scene validate --scene content/authoring/first_light.scene.json
$B/sovereign-cli scene apply --scene content/authoring/first_light.scene.json \
                             --commands examples/place_banner.command_batch.json   # dry run: changes nothing

V=build/dev-vulkan/bin
$V/sovereign-editor  --scene content/authoring/first_light.scene.json
$V/sovereign-runtime --scene content/authoring/first_light.scene.json
```

To commit an edit, copy the example batch, give it a new `idempotency_key` and `request_id`, set
`"dry_run": false`, and apply it. Re-sending the same batch replays the recorded result; changing its
content under the same key is rejected; an outdated `base_revision` is rejected with conflict details.
While the editor has a scene open, CLI writes to it are refused (exit 3).

## Map

- `AGENTS.md`: rules for agents working here.
- `docs/architecture/`: conventions, command protocol and durability, logging/errors, modules.
- `docs/decisions/`: ADRs.
- `docs/VALIDATION_MATRIX.md`: what G0/G1 must prove; `docs/reports/`: what was proven.
- `third_party_notices/`: upstream licences of the pinned dependencies.
