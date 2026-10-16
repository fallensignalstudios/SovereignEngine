# Caelis handoff: Sovereign Engine / First Ignition

**For:** Caelis  
**Owner:** Matthew  
**Assignment:** Implement G0 and G1 of `Sovereign_Engine_TDD_v0.1.md`.  
**Status of this packet:** Specification and contract fixtures only. No engine exists in this packet, and no native/GPU test has been run for it.

## The brief

We are investigating a complete standalone engine for the Sovereign Call projects: our own editor, renderer, runtime, gameplay framework, content pipeline, and native cinematic tools. This is not an Unreal plugin, wrapper, rebrand, or replacement for the production project today.

Your first job is narrower and concrete: **build a small native editor that creates, edits, saves, reloads, and runs an original 3D scene, with the same edits available through a validated command interface.**

The full TDD contains the destination. Implement only the first two gates now. Begin implementation rather than writing a second feasibility essay.

## Read in this order

1. This handoff.
2. `AGENTS.md`.
3. TDD Sections 1-11, 19, 21-22, and 26.
4. `docs/VALIDATION_MATRIX.md` and the JSON schemas/examples.
5. Remaining TDD sections to preserve future module boundaries, not to start those systems immediately.

## Isolation and permissions

Create an independent local `SovereignEngine` repository beneath the already-authorized development workspace, outside all production game and Unreal source/content trees. Verify the resolved path first. If an engine repository already exists, inspect its status and preserve its work rather than initializing over it. Use an isolated research branch such as `research/first-ignition`.

Do not modify ProjectVelkorran, Origins, Battles of the Reach, the MMO, GASPALS, Cinemafy, Holofy, Grungify, any Unreal installation, production saves, or unrelated repositories. Do not create a public remote, publish a release, choose a public license, buy services, or consume paid cloud resources.

Do not copy Epic engine implementation code or assume the current asset library is portable. Use original primitives and original shaders for the first scene. Obtain independent dependencies from their upstream sources with their applicable notices.

## Defaults to use

Use C++20, CMake/Ninja/CTest, SDL3 for platform/window/input, EnTT behind our world API, JSON authoring documents, Dear ImGui for tool UI, and **one native Vulkan 1.3 backend**. Use volk and VMA rather than writing those helpers ourselves. Use HLSL compiled offline by a pinned DXC build to SPIR-V.

Do not introduce D3D12, Metal, a second full rendering framework, C++ modules, Rust, C#, a general scripting VM, networking, a plugin marketplace, or an elaborate job system for this assignment. Libraries assigned to later gates need not be fetched or linked yet.

The Vulkan-first choice supersedes the earlier conversational D3D12-first sketch. A different choice needs a concrete compatibility finding and a recorded decision, not silent substitution.

## First: establish the machine and build

Record OS, CPU architecture, compiler versions, CMake/Ninja, Vulkan loader/SDK, physical GPU and driver, DXC availability, display access, and relevant write permissions. Never rely on remembered hardware details.

Pin dependencies and build settings. Provide a headless configuration that compiles/tests scene, math, command, and serialization behavior without creating a graphics device. Provide a graphics configuration for the real Vulkan path when the host supports it.

Expected build interface to implement and document:

```sh
cmake --preset dev-headless
cmake --build --preset dev-headless
ctest --preset dev-headless --output-on-failure

cmake --preset dev-vulkan
cmake --build --preset dev-vulkan
ctest --preset dev-vulkan --output-on-failure
```

These are required target interfaces, not commands claimed to exist or have been executed in this packet. Make presets match the actual supported toolchain and document platform prerequisites. `dev-headless` must not require Vulkan/SDL graphics initialization.

## Implement the first usable loop

**Scene and math:** UUID-based identity; strict schema validation; right-handed Y-up meters; explicit camera/projection conventions; parent-before-child transforms; no silent unknown-field loss.

**Commands:** Typed create/rename/transform/material/delete operations; staged whole-batch validation; scene revision checks; writer lock; idempotency keyed to request content; atomic save; actual undo/redo. GUI and CLI call the same services.

**Renderer:** SDL window, real Vulkan device/swapchain, depth-tested primitives, camera, simple materials, screenshot/readback, and safe resize/minimize/shutdown. One queue, small frame ring, explicit resource lifetime. No triangle-window-only completion claim.

**Editor:** Scene hierarchy, viewport, inspector, selection, save/reopen, undo history, errors, and isolated play/stop. Numeric transform controls are acceptable before a polished gizmo.

**Runtime:** Separate `sovereign-runtime` executable opens the saved scene without the editor. G1 may read validated authoring JSON; cooked-only runtime distribution is a G2 requirement.

**First Light:** An original primitive-built ceremonial hall with a walkway, columns, and raised platform. Bronze, dark red, charcoal, and restrained emissive accents. It need not look like Unreal yet. It must genuinely exist as editable scene data rather than hard-coded scenery or a background image.

Expected CLI behavior to implement:

```sh
sovereign-cli --version
sovereign-cli scene validate --scene content/authoring/first_light.scene.json
sovereign-cli scene apply --scene content/authoring/first_light.scene.json --commands examples/place_banner.command_batch.json
sovereign-runtime --scene content/authoring/first_light.scene.json
```

The example command batch is a **dry run** by default. Demonstrate that it leaves the scene unchanged. Then explicitly create a new committed request with `dry_run: false`, capture its result/diff, and test retry behavior. Do not report a dry run as an applied edit.

The CLI must refuse an offline write while the editor holds the scene writer lock. Do not build live network RPC merely to avoid implementing safe offline locking.

## Validation that cannot be skipped

Use `docs/VALIDATION_MATRIX.md` as the gate checklist. At minimum, prove:

- A clean build and actual native tests on the recorded host.
- Correct transform hierarchy, projection, culling, depth, and picking on a physical GPU.
- Equivalent GUI/CLI authored state and a real save-close-reopen cycle.
- Failed-batch rollback, stale-revision rejection, duplicate-delivery behavior, and changed-content/idempotency-key rejection.
- Undo/redo restore content while revisions remain monotonic.
- Play modifies only the runtime world, never the authored document.
- A separate runtime executable opens the saved scene without editor code.
- Resize/minimize/restore and repeated open/save/play/stop do not produce validation errors, corruption, or persistent resource growth.
- No production repository or asset was changed.

A render screenshot, a compiled executable, a software-rendered frame, and a Null backend are different evidence classes. State which one you actually have.

## Handling blockers

If graphics cannot execute on this host, complete useful headless work and report GPU qualification as blocked. Provide the binary/build procedure and an exact GPU test procedure for an authorized suitable machine. Do not mark G1 complete, generate a fake screenshot, or substitute a mocked renderer.

If an ordinary implementation detail is unresolved, choose the simplest approach consistent with the TDD and record it. Stop and report for paid resources, production edits, unclear third-party rights, a second graphics backend, major contract changes, or publication. Do not stall on aesthetic polish or ask Matthew to decide every small code choice.

If this execution ends before the gate is complete, preserve a buildable checkpoint where practical and report precise remaining work. Do not claim you will continue autonomously after execution stops unless the actual host workflow provides that capability.

## Required final report

Write `docs/reports/G0_G1_REPORT.md` with:

1. Commit, host/toolchain, GPU/driver, dependency-lock hash, and active backend.
2. Implemented modules and what each actually does.
3. Commands executed and their results, with log paths.
4. Every validation ID marked passed, failed, blocked, or not run.
5. Real editor/runtime captures, scene before/after, command request/result, and diff.
6. Known defects, unsupported features, and any deviations.
7. Confirmation of production isolation and no unapproved spending/publication.
8. The single next recommended task, with its acceptance check.

**Stop after the G0/G1 report. Do not begin combat, motion matching, native Cinemafy, or game migration in this assignment.**

The objective is not to claim we replaced Unreal. The objective is to put the first working piece of an independent engine in Matthew's hands.
