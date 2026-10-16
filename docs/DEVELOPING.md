# AGENTS.md: working rules for agents in this repository

> **Reconstructed.** The First Ignition packet listed an `AGENTS.md`, but only the TDD (v0.1) was
> supplied. This file is rebuilt from TDD sections 1, 3, 4, 19, 22 and 23 plus what this repository
> actually contains. If Matthew's original exists, it takes precedence; reconcile and delete this note.

## What this repository is

Sovereign Engine is a **separate research engine** (TDD §2-3): our own editor, renderer, runtime and
tools. It is not an Unreal plugin, wrapper or migration path. The active assignment is **G0 + G1
only** (TDD §22). Do not start G2+ systems (asset import, PBR, physics, animation, combat,
cinematics, networking) without an explicit new assignment.

## Hard boundaries (TDD §3)

- Work only inside this repository. Never modify ProjectVelkorran, Origins, Battles of the Reach,
  the MMO, GASPALS, Cinemafy, Holofy, Grungify, any Unreal installation, or production saves.
- No Epic engine code, no Unreal headers/modules/tools, no `.uasset`/`.umap` ingestion.
  `ctest -R static.no_unreal_dependency` enforces the code side.
- No paid services, public remotes, releases, telemetry or licence choices on Matthew's behalf.
- Original content only (primitives + original shaders). No downloaded art.
- No placeholder success: a CPU Vulkan device, a dry run, a mocked path or a skipped test is
  reported as exactly that. Never mark a GPU gate complete from a software-rendered result.
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
`scripts/build_dxc_from_source.sh` (ADR-015). Window tests run on a real display, else in a private
Xvfb+openbox display, else they are **skipped** (exit 77), never passed.

## Where things are

| Path | Module |
|---|---|
| `engine/core` | errors, UUID, SHA-256, logging, durable file I/O + writer lock, strict JSON, math, PNG |
| `engine/world` | scene document model, reflection descriptors, strict reader/writer, diff, EnTT `World`, primitives |
| `engine/commands` | typed operations, staging, `SceneSession` (revisions, idempotency, undo/redo), `SceneStore` (journal, lock, atomic commit, recovery) |
| `engine/runtime` | fixed-step `Simulation`, game-module seam; `app/` = `sovereign-runtime` |
| `engine/platform` | SDL3 window, events, input action map |
| `engine/rhi/vulkan` | Vulkan device/swapchain/resources (volk + VMA) |
| `engine/renderer` | R0 renderer (public API has no Vulkan types), HLSL shaders |
| `editor/authoring` | `EditorDocument`: the only service GUI panels call |
| `editor/application`, `editor/panels` | `sovereign-editor` (Dear ImGui tool UI) |
| `tools/cli` | `sovereign-cli` |
| `games/sandbox` | original sandbox game module used by play mode and the runtime |
| `content/authoring`, `content/source` | First Light scene and the batch that builds it |
| `schemas/`, `examples/` | wire contracts and the example batch |
| `tests/` | `sovereign-tests` (headless), `sovereign-render-tests` (Vulkan), automation scripts |

## Rules of the road

- **One authoring path.** GUI, CLI and agents mutate scenes only through `cmd::SceneSession`
  (via `SceneStore` or `editor::EditorDocument`). Never edit scene JSON or ECS storage directly.
- **Reflection descriptors are the field allowlist.** Add a field once in
  `engine/world/src/reflection.cpp`; parsing, writing, the inspector and command permissions follow.
  Update `schemas/scene.schema.json` too; `scene.schema_files_match_reflection_descriptors` fails otherwise.
- **Conventions are fixed** (docs/architecture/conventions.md): right-handed, +Y up, meters,
  cameras look down -Z, M = T*R*S with column vectors, quaternions `[x, y, z, w]`, reversed-Z
  Vulkan depth with the Y flip in the projection only, linear colour with one sRGB encode at output.
- **Evidence vocabulary** (TDD §1.3): specified, implemented, compiled, headless-tested,
  GPU-tested, human-reviewed, qualified, blocked/not run. Use it in every report.
- **Every capture names its source.** Use `render::CaptureProvenance`; PNGs carry tEXt chunks with
  source, device, evidence class, validation state and commit.
- Validation layers + synchronization validation stay on in tests. A new validation message is a
  bug to fix or a documented waiver (exact message, cause, owner), never a suppression.
- Small commits on `research/first-ignition`; message bodies say what was verified.
