# Developing Sovereign Engine (repository guide)

The rules for agents are the owner's [`AGENTS.md`](../AGENTS.md) (verbatim from the First Ignition
packet; ADR-017). This guide adds what the repository itself contains and how to work in it. Where the
two differ, `AGENTS.md` wins.

## What this repository is

Sovereign Engine is a **separate research engine** (TDD §2-3): our own editor, renderer, runtime and
tools. It is not an Unreal plugin, wrapper or migration path. The active assignment is **G0 + G1
only** (TDD §22). Do not start G2+ systems (asset import, PBR, physics, animation, combat,
cinematics, networking) without an explicit new assignment.

## Hard boundaries (TDD §3, AGENTS.md)

- Work only inside this repository. Never modify ProjectVelkorran, Origins, Battles of the Reach,
  the MMO, GASPALS, Cinemafy, Holofy, Grungify, any Unreal installation, or production saves.
  `scripts/isolation_check.sh` records the before/after state (G0-01).
- No Epic engine code, no Unreal headers/modules/tools, no `.uasset`/`.umap` ingestion.
  `static.no_unreal_dependency`, `static.<exe>_link_dependencies` and
  `vulkan.loaded_libraries_are_system_only` check source, link and run time (G0-06).
- No paid services, public remotes, releases, telemetry or licence choices on Matthew's behalf.
- Original content only (primitives + original shaders). No downloaded art.
- No placeholder success: a CPU Vulkan device, a dry run, a mocked path or a skipped test is
  reported as exactly that. Never mark a GPU gate complete from a software-rendered result.
- The packet's contract files are byte-identical copies (`static.packet_files_verbatim`). Changing
  a schema, the matrix, an example or `AGENTS.md` is a contract change: stop and ask.
- Stop and report for: production edits, paid resources, unclear third-party rights, a second
  graphics backend, major contract changes, publication.

## Build and test

```
cmake --preset dev-headless && cmake --build --preset dev-headless && ctest --preset dev-headless
cmake --preset dev-vulkan   && cmake --build --preset dev-vulkan   && ctest --preset dev-vulkan
ctest --preset gpu-qualify            # physical-GPU qualification: refuses CPU Vulkan devices
cmake --preset dev-headless-asan ...  # ASan + UBSan (GCC/Clang)
```

`dev-headless` never needs a display, SDL or Vulkan (`static.headless_*_has_no_graphics` checks
the binaries). `dev-vulkan` on Linux needs DXC built from source once:
`scripts/build_dxc_from_source.sh` (ADR-015); configure fails with the path if the selected DXC is
missing or cannot run. Window tests run on a real display, else in a private Xvfb+openbox display,
else they are **skipped** (exit 77), never passed. Every Vulkan test runs with
`SOV_REQUIRE_VALIDATION=1`: a host without the Khronos validation layer fails them with a diagnostic.

Useful switches: `--device NAME` (editor, runtime) or `SOV_VULKAN_DEVICE` picks a device by name
substring; `SOV_REQUIRE_PHYSICAL_GPU=1` refuses CPU implementations; `sovereign-runtime
--perf-report OUT.json` writes the performance report (frame/CPU/GPU percentiles, memory, settings).

## Evidence scripts

| Script | Produces |
|---|---|
| `scripts/clean_build_evidence.sh` | clean configure/build/test of both presets with fresh fetches |
| `scripts/fresh_clone_evidence.sh` | the same from a fresh clone of the committed tree |
| `scripts/collect_evidence.sh` | offline rebuild, sanitizers, CLI demo, captures, editor/GUI sessions, failure paths, dependency inspection, performance reports, gpu-qualify, isolation before/after |
| `scripts/cli_demo_evidence.sh` | the packet example as a dry run, then committed, retried, conflicted, undone, redone, locked |
| `scripts/gui_real_input_check.sh` | real X11 input to the editor (XTEST), CLI refused while the editor holds the lock |
| `scripts/failure_paths_check.sh`, `scripts/configure_failure_paths.sh` | missing prerequisites and partial initialization fail with diagnostics |
| `scripts/check_contracts.py` | scenes, batches and results against the schemas with an independent validator |

## Where things are

| Path | Module |
|---|---|
| `engine/core` | errors, UUID, SHA-256, logging, durable file I/O + writer lock, strict JSON, math, PNG, process memory |
| `engine/world` | scene document model, reflection descriptors, strict reader/writer, diff, EnTT `World`, primitives |
| `engine/commands` | typed operations, staging, `SceneSession` (revisions, idempotency, undo/redo), `SceneStore` (journal, lock, atomic commit, recovery) |
| `engine/runtime` | fixed-step `Simulation`, game-module seam; `app/` = `sovereign-runtime` |
| `engine/platform` | SDL3 window, events, input action map |
| `engine/rhi/vulkan` | Vulkan device/swapchain/resources (volk + VMA) |
| `engine/renderer` | R0 renderer (public API has no Vulkan types), HLSL shaders |
| `editor/authoring` | `EditorDocument`: the only service GUI panels call |
| `editor/application`, `editor/panels` | `sovereign-editor` (Dear ImGui tool UI) |
| `tools/cli` | `sovereign-cli` |
| `tools/validate_contract_examples.py` | the packet's own contract checker (verbatim) |
| `games/sandbox` | original sandbox game module used by play mode and the runtime |
| `content/authoring` | First Light (the packet scene, imported with `sovereign-cli scene import`) |
| `schemas/`, `examples/` | the owner's wire contracts and examples (verbatim); `command_result.schema.json` is engine-defined |
| `docs/packet/` | the First Ignition packet as supplied, with its manifest |
| `tests/` | `sovereign-tests` (headless), `sovereign-render-tests` (Vulkan), fixtures (Axis Lab), automation scripts |

## Rules of the road

- **One authoring path.** GUI, CLI and agents mutate scenes only through `cmd::SceneSession`
  (via `SceneStore` or `editor::EditorDocument`). Never edit scene JSON or ECS storage directly.
- **Reflection descriptors are the field allowlist.** They mirror the owner's scene schema;
  `scene.schema_files_match_reflection_descriptors` fails if they diverge. Parsing, writing, the
  inspector and command permissions all follow the descriptors.
- **Conventions are fixed** (docs/architecture/conventions.md): right-handed, +Y up, meters,
  cameras look down -Z, M = T*R*S with column vectors, quaternions `[x, y, z, w]`, reversed-Z
  Vulkan depth with the Y flip in the projection only, linear colour with one sRGB encode at output,
  R0 fixed lighting (the contract has no lights).
- **Evidence vocabulary** (TDD §1.3): specified, implemented, compiled, headless-tested,
  GPU-tested, human-reviewed, qualified, blocked/not run. Use it in every report.
- **Every capture names its source.** Use `render::CaptureProvenance`; PNGs carry tEXt chunks with
  source, device, evidence class, validation state and commit.
- Validation layers + synchronization validation stay on in tests. A new validation message is a
  bug to fix or a documented waiver (exact message, cause, owner), never a suppression.
- Small commits on `research/first-ignition`; message bodies say what was verified.
