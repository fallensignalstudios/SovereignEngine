# Sovereign Engine
## Technical Design Document v0.1
### First Ignition: a standalone engine, built one verified capability at a time

**Prepared for:** Matthew and Caelis  
**Project:** Sovereign Engine, provisional name  
**Document date:** October 16, 2026  
**Status:** Proposed architecture and implementation specification. No engine implementation is included or claimed.  
**Immediate assignment:** G0 and G1 only. See `CAELIS_FIRST_IGNITION.md`.  
**Long-term intent:** A complete, independently implemented replacement for Unreal within the Sovereign Call projects, with our own editor, renderer, runtime, and gameplay systems.  
**Production boundary:** This authorizes a separate research project. It does not authorize migration or modification of any existing game.

> Build something real enough to judge. The first success is a small scene that we can create, inspect, save, reopen, and run without Unreal. The long-term ambition stays large; each implementation gate stays testable.

## Contents

1. Document authority and evidence
2. Product definition and project profiles
3. Non-negotiable boundaries
4. Initial technology decisions
5. Repository and module architecture
6. Mathematical and data conventions
7. Runtime, scheduling, and ownership
8. World model and reflection
9. Asset pipeline and content portability
10. Renderer
11. Editor and transactional authoring
12. Physics and character movement
13. Animation and motion matching
14. Gameplay foundation and combat
15. AI, navigation, and companions
16. Narrative and native Cinemafy
17. Input, cameras, audio, effects, and game UI
18. Saves, checkpoints, and streaming
19. Agent interface and security
20. Platform profiles and networking boundaries
21. Reference scenes and performance qualification
22. Implementation gates
23. Engineering effort and working method
24. Risk register
25. Migration and continuation decisions
26. Open decisions and source register

---

# 1. Document authority and evidence

## 1.1 What this document is

This is an engineering proposal and implementation contract, not evidence that the technology works. **Must** identifies a requirement; **should** identifies a default that may change through a recorded architecture decision; **deferred** means do not implement it in the first assignment.

The owning decision maker is Matthew. Caelis owns implementation, technical investigations, and honest qualification reports. An agent may propose changes to this document, but must not silently lower an acceptance threshold to mark its own implementation complete.

The source of truth for current engine behavior will be the code and reproducible tests in the new engine repository. The source of truth for Sovereign Call canon and combat tuning remains the approved game design, not this engine TDD.

## 1.2 Context used and limitations

The accessible project document was `Sovereign Call_ Origins Technical Design Document-20251209034722.md`. It was used as a legacy inventory of system families: survivability, stamina, poise, Echo, abilities, combat, dialogue, quests, relationships, world state, animation, AI, UI, saves, and streaming. Its numbered phase order, old balance values, class roster, and claims of existing implementation are **not** adopted here. A newer v2 outline mentioned in prior discussions was not retrieved during preparation. Failure to retrieve it does not establish that it does not exist. [P1]

The brief additionally calls for native cinematic direction, agent-operable editing, a standalone game runtime, and support for multiple Sovereign Call game types. Current repositories, Caelis's active machine, GPU, installed compilers, asset entitlements, and build branches were not inspected. G0 must establish those facts before making platform or reuse claims.

Public foundation libraries and relevant Epic/Fab terms were checked against the primary sources in Section 26. Exact dependency revisions remain to be pinned and tested in G0. No compatibility, licensing, or performance conclusion is inferred merely from a library being publicly available.

## 1.3 Evidence vocabulary

Every feature report must distinguish:

| State | Meaning |
|---|---|
| Specified | Behavior exists only in documents or contracts. |
| Implemented | Code exists; execution may still be unverified. |
| Compiled | The named target built with the recorded toolchain. |
| Headless-tested | The specified non-graphics tests actually executed. |
| GPU-tested | The graphics path executed on a named physical GPU and driver. |
| Human-reviewed | Matthew reviewed the named artifact or build. |
| Qualified | All acceptance checks for that gate and platform passed. |
| Blocked / not run | A prerequisite prevented verification. Not a pass. |

A Null graphics backend does not prove rendering. A software Vulkan implementation does not qualify physical-GPU performance. A screenshot does not prove save/load, editing, packaging, animation, or gameplay.

# 2. Product definition and project profiles

## 2.1 Engine ownership

We own the engine's renderer and render graph, scene model, editor, asset/cook pipeline, scheduling, authoring API, gameplay framework, animation integration, cinematic system, packaging, and diagnostics.

We may use independently licensed libraries for platform access, memory allocation, physics, parsers, ECS storage, navigation, audio, and tooling UI. Reusing a physics library or graphics API does not turn the engine into an Unreal wrapper. Using another complete engine as a hidden runtime would violate this brief.

No Unreal executable, Unreal SDK, Unreal runtime module, `.uasset` reader, or Epic source checkout is required to configure, build, test, edit, or run Sovereign Engine.

## 2.2 Three deliverables, one shared foundation

**Runtime:** Runs cooked game content independently of the editor.

**Editor:** Authors and inspects scene, material, gameplay, and cinematic data. Uses the same engine services as the runtime, with a separate authoring world.

**Toolchain:** Imports, validates, cooks, tests, packages, and reports from a command line. It must remain useful when no interactive editor or graphics device is available.

## 2.3 Game profiles

These are future consumers, not simultaneous first milestones.

| Profile | Distinct requirements | Earliest meaningful proof |
|---|---|---|
| Origins | Third-person action, character animation, melee/ranged combat, companions, dialogue, cinematic staging, authored missions | A short self-contained combat and cinematic slice |
| Battles of the Reach | Bounded 5v5 encounters, ability scheduling, readable UI, roster data, mobile constraints | A ten-unit desktop arena, followed by real mobile packaging and profiling |
| Settlement/city builder | Isometric camera, placement validation, many simple agents, resource simulation, persistent buildings | A small placement-and-resource sandbox after shared foundations work |
| MMO | Authoritative servers, replication, interest management, persistent services, abuse resistance, live operations | A separate networking qualification program, not a local crowd demo |

No game-specific names or faction rules belong in the platform, renderer, asset system, or core ECS. Generic engine capabilities may understand attributes, actions, tags, factions, and relationships. Values such as Echo, Dominion, Reformation, Tarrik, and Selene live in game modules and data.

## 2.4 What first success looks like

Launch our editor. Open a compact original sci-fi hall. Select a column, move it, undo that move, change a material parameter, save, close, and reopen. Load the same saved scene in a separate runtime executable. Perform a scoped scene edit through a validated command batch. Receive a report showing exactly what changed.

That is G1. Combat, imported animated characters, motion matching, and advanced lighting come later. The hall is an original technical test scene, not a rebuilt production mission or a claim of visual parity with Unreal.

# 3. Non-negotiable boundaries

1. **Separate repository and working directory.** Do not create this inside ProjectVelkorran or any other production game's source or content tree. Do not alter production branches, asset files, editor configurations, saves, plugins, or engine installations.
2. **No hidden technology substitution.** Do not wrap Unreal, Godot, Unity, an existing complete engine, or an unapproved renderer and call it our own implementation.
3. **Independent implementation.** No copying Epic engine implementation code. Use original code and appropriately licensed independent dependencies. Reusing our own work requires ownership and dependency review, not assumption. [S14]
4. **No automatic migration.** Even a successful prototype does not authorize transferring Origins or changing the production workflow.
5. **No automatic spending or publication.** No paid services, purchased assets, cloud GPU jobs, public repository creation, releases, or external telemetry without Matthew's authorization.
6. **No placeholder success.** A fallback image, Null backend, mocked import, or stubbed test must identify itself and cannot satisfy the real feature gate.
7. **Evidence before expansion.** Finish and qualify the active slice before starting an unrelated subsystem.
8. **Source assets stay private by default.** Only original or explicitly cleared assets enter the research project. Public code and public content are separate release decisions.

# 4. Initial technology decisions

## 4.1 Selected baseline

The baseline is **C++20, CMake with Ninja presets, SDL3 for window/input, and a native Vulkan renderer**. This is a deliberate refinement of the earlier conversation's D3D12-first suggestion. A single Vulkan backend offers a practical Windows/Linux development route while we qualify Windows as the first game-delivery target. It avoids writing two native graphics backends immediately.

The first desktop graphics profile is Vulkan 1.3 with the explicitly requested feature set recorded at startup. Dynamic rendering and synchronization2 are part of that design; feature, queue, format, and device support must still be queried and validated. [S2]

A graphics abstraction is a small engine-owned boundary, not a universal GPU framework. We do not add D3D12, Metal, ray tracing, or bindless resource requirements until there is a measured reason.

| Area | Initial choice | First use / constraint |
|---|---|---|
| Language | C++20, conventional headers and translation units | No C++ modules, multiple native implementation languages, or hot-reload ABI in G1 |
| Build | CMake + Ninja, CTest | Pin supported toolchain and dependency commits in G0 |
| Platform | SDL3 | Windowing, event collection, input; not SDL's renderer [S1] |
| Graphics | Native Vulkan 1.3 | One real backend; engine-owned passes and shaders [S2] |
| Vulkan loading / memory | volk and Vulkan Memory Allocator | Do not build an API loader or device-memory allocator from scratch [S3, S4] |
| Shaders | HLSL compiled offline with DXC to SPIR-V | Pin compiler, flags, binding conventions, and reflection contract [S5] |
| ECS storage | EnTT | Keep its internal entity handles out of persisted/public contracts [S6] |
| Editor UI | Dear ImGui docking build, pinned | Tool UI only; not a finished player-facing accessibility solution [S7] |
| Authoring serialization | JSON using a pinned parser, proposed nlohmann/json | Strict schemas, stable ordering, bounded parsing [S13] |
| Asset interchange | glTF 2.0 / GLB, cgltf parser | Narrow supported subset; parsing is not a complete importer [S8, S9] |
| Physics | Jolt | Rigid-body, collision, and character integration from G3 [S10] |
| Navigation | Recast + Detour | Build and query navmeshes from G3; engine owns behavior [S11] |
| Audio | miniaudio | Playback and mixing integration, initially WAV [S12] |
| Tests | CTest and a small native assertion harness | Adopt a larger test framework only when useful |
| Scripting | Compiled C++ game modules and data first | Lua is a later proposal, not a G1 dependency [S15] |

A library listed for a future gate must not be linked into the first executable merely because it appears in this table. G1 requires only its active foundation dependencies.

## 4.2 Dependency controls

Use one dependency acquisition method: CMake FetchContent against immutable commits for source dependencies, with verified prebuilt tool packages where appropriate. Record exact revisions and checksums. Do not track `main`, `master`, floating tags, or an implicit latest version in a reproducible build.

`dependencies.lock.json` must record origin, selected revision, archive checksum when applicable, license-file path/hash, optional transitive dependencies, build options, and redistribution notes. Keep third-party notices. A dependency lock is a record of the selected stack, not a claim that every upstream license is interchangeable.

Build directory and generated dependencies stay outside authored content. A clean checkout must bootstrap from documented prerequisites. A populated dependency cache must allow subsequent normal builds without network access. Updates require a dedicated change, build/test evidence, and lockfile refresh.

## 4.3 Platform truth

G0 records the actual host. Windows and Linux can host the initial Vulkan work when required support exists. Portable core, scene, command, and cook tests should also compile without graphics on a supported macOS toolchain. A native macOS editor is **not** promised by that fact.

When Caelis has no supported physical GPU, he can finish the headless portion and prepare an executable/test procedure for a suitable host. He must report graphics qualification as blocked. Do not silently install a different engine, substitute OpenGL, or start a second backend to conceal the limitation.

# 5. Repository and module architecture

## 5.1 Proposed layout

```text
SovereignEngine/
  CMakeLists.txt
  CMakePresets.json
  dependencies.lock.json
  AGENTS.md
  cmake/
  docs/
    architecture/
    decisions/
    reports/
  engine/
    core/
    platform/
    world/
    assets/
    rhi/
      null/
      vulkan/
    renderer/
    physics/
    animation/
    navigation/
    audio/
    gameplay/
    narrative/
    cinematics/
    ui/
    runtime/
  editor/
    application/
    panels/
    authoring/
  tools/
    cli/
    cooker/
    validators/
  games/
    sandbox/
    reach_arena/          # future independent sample
  content/
    source/
    authoring/
  schemas/
  tests/
    unit/
    integration/
    rendering/
    fixtures/
  scripts/
  third_party_notices/
```

This is the target organization. Do not create dozens of empty classes, hollow modules, fake plugins, or directories that imply implementation. Add modules when their first real consumer exists.

## 5.2 Dependency direction

```text
Editor shell ---> Authoring services ---> World / Assets / Commands
Runtime app ---------------------------> Runtime orchestration
CLI ---------> Validators / Cooker ---> World / Assets / Core

Game modules ---> Gameplay / Narrative / Cinematics / World
Runtime -------> Gameplay / Physics / Animation / Navigation / Audio
Renderer ------> Render snapshot / RHI / Assets
Vulkan RHI ----> Vulkan / volk / VMA / platform surface adapter
World ---------> Core / ECS storage / reflection descriptors
Everything ----> Core
```

Core cannot depend on the editor, a game, a renderer, or a platform graphics SDK. Renderer cannot read live mutable gameplay state; it consumes a render snapshot. A game module does not include Vulkan headers. Public asset handles do not carry `VkImage`, `JPH::BodyID`, or `entt::entity`.

The authoritative schema and validation library must be linked by editor, CLI, and cooker rather than reimplemented in three places.

## 5.3 Executable targets

**`sovereign-editor`:** Interactive authoring and an isolated play session.

**`sovereign-runtime`:** A standalone application loading a project/game manifest and scene or cooked package.

**`sovereign-cli`:** Validation, command batches, asset cooking, test orchestration, inspection, and later packaging.

**`sovereign-tests`:** Native automated checks. Renderer checks are separate and clearly tagged.

G1 runtime may read validated authoring JSON. Cooked-only distribution is introduced and required at G2. G1 must already run in a separate executable without editor UI code.

## 5.4 ABI and extension policy

Use static or normally linked engine/game libraries first. Do not promise binary compatibility across compiler or engine revisions. Data schemas and command protocol versions are the early compatibility boundary.

A future plugin system should use explicit API versions and handles, not exported STL containers across an unstable DLL boundary. General plugin loading and C++ hot reload are deferred. Restarting a small runtime is acceptable while the module ABI is unsettled.

# 6. Mathematical and data conventions

Conventions must be implemented in tests before importing production-like assets.

| Concern | Engine convention |
|---|---|
| Distance | Meters |
| Time | Seconds at interfaces; integer ticks for simulation scheduling |
| Basis | Right-handed, +Y up, +X right; cameras look along local -Z |
| Rotation | Unit quaternion, serialized `[x, y, z, w]`; editor displays degrees |
| Transform | Column-vector convention, `M = T * R * S` |
| Matrix storage | Defined by engine math types and shader packers, never inferred from notation |
| Physics scale | Positive unit scale for animated characters and dynamic rigid bodies initially |
| Render depth | Vulkan depth range `[0,1]`; reversed-Z from the first 3D camera |
| Reversed-Z state | Near maps to 1, far approaches 0; clear depth 0; compare greater-or-equal |
| CPU color | Linear floating point for lighting and material factors |
| Color textures | Base color and emissive textures decoded from sRGB; data maps remain linear |
| Presentation | Linear HDR scene buffer, explicit tone mapping, one output encoding step |
| Persisted IDs | Canonical lowercase UUID strings, independent of runtime slot handles |

glTF uses a right-handed, Y-up coordinate system and meters. Interchange adapters must still check camera, transform, material, and animation conventions rather than assuming no conversion is needed. [S8]

### Required conversion tests

An asymmetric axis fixture has differently shaped X/Y/Z markers, a front-facing triangle, a camera, a textured quad, a child transform, and eventually an animated joint. Test culling, texture orientation, normal mapping, world/local transforms, nonuniform scale handling, and camera projection together.

Vulkan framebuffer orientation is solved **once** in the projection/viewport adapter. Document whether the implementation flips the projection Y or uses a negative-height viewport; do not do both. Lock triangle front-face convention using the fixture. Shader matrix packing and multiplication order must be tested independently of CPU memory layout.

Unreal-origin assets require an explicit source-basis and unit conversion. Do not merely divide positions by 100: handedness, winding, tangents, rotations, skeleton roots, cameras, and animation translations can also need conversion. Source import settings must record the applied conversion.

### Large coordinates

G1 scenes use local floating-point coordinates. The scene format reserves a region identity, but does not pretend large-world precision is solved. Later, a world position may be represented as an integer region/cell coordinate plus a local position; physics and rendering operate on a local origin. This needs its own streaming and precision tests before large planets or MMO zones are qualified.

# 7. Runtime, scheduling, and ownership

## 7.1 Time model

The initial simulation uses a fixed **60 Hz** tick. This is a proposed default, not a universal game requirement. Rendering is variable rate and interpolates between published simulation snapshots.

The interactive loop caps catch-up work at four simulation steps per render iteration, records backlog, and applies an explicit overload policy. Offline tests advance an exact requested number of ticks without wall-clock catch-up. Cinematic export uses an explicit rational frame rate and authoritative sequence time.

A fixed timestep and a seeded random generator do **not** by themselves guarantee determinism. Ordering, floating-point behavior, library configuration, physics, and asynchronous delivery also matter. Initial repeatability promises are restricted to named tests on the same pinned build/configuration. Jolt's own determinism conditions must be respected and tested if that property is required. [S10b]

## 7.2 Tick pipeline

The starting schedule is single-writer and intentionally explicit:

1. Consume timestamped input, authorized commands, and queued world changes at the tick boundary.
2. Advance gameplay action timers and select state transitions, including valid cancel/interrupt requests.
3. Evaluate AI intent and action requests against the current observed state.
4. Sample the action/animation segment for this tick and extract the requested root-motion delta.
5. Resolve movement authority and produce one movement request per controlled character.
6. Execute character collision/movement and physics according to the integration contract.
7. Build world-space poses and swept combat queries from the resolved transforms.
8. Resolve hits, attributes, effects, deaths, quest events, and deferred structural changes.
9. Publish presentation events and an immutable render/audio-facing snapshot.

The physics/character substep ordering must be documented and tested against the selected Jolt integration. This schedule must not be treated as permission to move a character twice.

Gameplay-active animation markers are processed across the entire traversed time interval, not only at a sampled frame. A short hit window cannot disappear just because a tick steps over it. Presentation interpolation never deals damage or advances cooldowns.

## 7.3 Threading

G1 runs engine state mutation on one owner thread. Background tasks can read immutable inputs and return results. No worker directly mutates the world, renderer resource registry, or authoring document.

Later, worker pools perform import, asset I/O, animation jobs, culling preparation, and other independent work. Mutations are applied through declared dependency barriers. Begin with ordinary queues and measured locks. Do not create a custom lock-free job system before there is evidence that it is needed.

GPU commands and resource retirement have explicit owners. Audio callbacks cannot wait on gameplay locks or allocate through unpredictable code paths.

## 7.4 Errors, shutdown, and memory

Use RAII, clear ownership, generation-checked handles, and descriptive failure results. An invalid content reference should produce an error with the affected asset/entity ID, not an unexplained crash.

Shutdown must stop new work, cancel/join workers, drain or retire GPU use safely, release device-dependent resources, and finally release platform objects. Include double-close, failed-initialization, device-loss, and partial-shutdown tests as relevant modules appear.

Do not optimize allocations everywhere in advance. Track persistent heaps, transient frame arenas, resource counts, high-water marks, and unbounded-growth candidates. A repeated load/unload loop should return close to its settled resource baseline, not leak a little more each iteration.

# 8. World model and reflection

## 8.1 Entities and identity

Use EnTT for in-memory component storage behind an engine world API. EnTT's implementation details are not a persistence contract. [S6]

Each authored entity has a stable UUID. Each live world maps that UUID to its own generation-checked runtime handle. Distinguish authoring-world, play-world, and runtime-world identities; an editor selection must not accidentally target a different world's entity with the same slot number.

A scene has a UUID, schema version, monotonically increasing authoring revision, a list of entities, and explicit asset references. Names are labels, not keys. Duplicate names are allowed; duplicate UUIDs are not.

## 8.2 Initial component set

G1 implements only `Name`, `Transform`, `PrimitiveMesh`, `MaterialParameters`, `Camera`, and optionally a simple `DirectionalLight` when the first lighting pass exists. `Transform` includes an optional parent UUID. The initial primitive set is box, plane, and cylinder; a box and plane are sufficient for the earliest render check.

G2 adds asset-backed mesh/material references, visibility bounds, and import metadata. G3 adds collider, character motor, animation player, attribute container, ability owner, and simple AI components.

Reflection descriptors must define field name, serialized type, schema version, default, validation constraints, display metadata, mutability, and whether a field is editor-only or persisted. The same descriptors power validation, inspector fields, and command allowlists. No raw pointer offset or arbitrary memory address is accepted from an agent.

## 8.3 Hierarchies and structural changes

A parenting change rejects cycles and missing parents before commit. Deleting an entity either rejects while dependents exist or explicitly includes the affected dependent operations in the same transaction. There is no hidden cascading delete in G1.

Changes to entity structure are applied at a safe world boundary. Iteration invalidation rules are tested. Transform propagation runs in parent-before-child order. Prefab instances and nested overrides are deferred until ordinary scenes are stable.

## 8.4 Serialization

Human-authored JSON is canonical source. Cooked runtime blobs are derived. Keep source asset, import recipe, authoring document, and cook artifact separate.

G1 stores one JSON scene document, sorted by stable UUID and canonical field order. Floating-point output must be finite and stable enough to avoid noisy diffs. Writing the same unmodified scene must not rewrite arbitrary formatting or increment revision unnecessarily.

Unknown schema versions fail clearly. Unknown component types cannot be silently discarded. A read-only inspection tool may preserve opaque data, but the writer must not overwrite a document it cannot fully understand.

A save operation writes a temporary file, flushes it, and performs a platform-appropriate atomic replacement on the same filesystem. It preserves the last known valid file on failure. Do not claim that a sequence of independent file renames is an atomic multi-file transaction.

## 8.5 Future collaborative format

One-file scenes are adequate for the first proof, not the end state. G5 evaluates scene manifests plus per-entity or per-cell documents and explicit prefab override records. A journal or generation-pointer approach must make multi-file commits recoverable. Format changes require an explicit migration tool, backups, and round-trip tests.

# 9. Asset pipeline and content portability

## 9.1 Pipeline

```text
Cleared source bytes + import recipe
             |
       Parse and validate
             |
   Normalize units, basis, and types
             |
  Produce engine intermediate asset
             |
 Cook for a named platform/profile
             |
 Hash-addressed artifact + manifest
             |
 Runtime asset registry and streaming
```

The editor is a client of this pipeline, not a second importer. A CLI cook must work without the editor process.

## 9.2 Source, derived data, and provenance

Every asset has a stable `AssetId`, asset kind, source path or URI within the project, source hash, import-recipe hash, dependency list, importer version, and license/provenance record.

The cook key includes source/dependency content hashes, importer and processor revisions, recipe, shader/compiler options when relevant, target profile, and cook-format version. A content change invalidates only dependent artifacts. Timestamps alone cannot establish that a cache entry is valid.

During development, a missing optional visual asset may show a clearly labeled error primitive. Shipping/qualification cooks fail on missing required references. A previous successfully cooked version stays usable if a new reimport fails.

## 9.3 Narrow glTF support first

The initial import target is GLB with triangle meshes, node transforms, metallic-roughness materials, bounded buffers, and PNG/JPEG texture inputs. cgltf provides parsing and validation utilities, but asset normalization, GPU upload, engine formats, material interpretation, and lifecycle remain ours. [S9]

G2 must explicitly handle or reject: accessor stride/range, sparse accessors, index component types, missing normals/tangents, zero-area primitives, alpha mode, double-sided materials, negative determinant transforms, and declared required extensions. Do not interpret an unsupported extension as default-looking valid content.

Initial support may reject sparse accessors, compressed geometry, skins, morphs, and complex material extensions with precise errors. Add each support path only with fixtures. Animated skeleton import arrives with G3, not through an untested checkbox in G2.

Apply per-import resource limits before allocation: maximum input size, total decoded buffer bytes, image dimensions, material count, node depth, joint count, and animation samples. Reject path traversal, absolute external paths, and unapproved network URIs.

## 9.4 Textures, materials, and skeletons

Texture import records usage, color space, channel mapping, normal-map convention, mip policy, and target compression. Base color alpha and material transparency require explicit handling. glTF's roughness and metallic channel convention must be translated deliberately. [S8]

For first static PBR, use ordinary textures and generated mip chains. Texture compression is a later named cook step; do not introduce a proprietary codec or platform restriction without recording it.

A skeleton asset stores joint hierarchy, bind transforms, inverse bind matrices, root selection, and stable joint identifiers. Clip assets store sample times, interpolation mode, channel targets, and root-motion policy. Validate bind-pose reconstruction before evaluating imported animation.

Retargeting and animation compression are separate features. Correct playback on the source skeleton comes first. A successful mesh import does not prove a retargeted character works.

## 9.5 Unreal portability matrix

| Existing material | Potential reusable input | What must be replaced or reviewed |
|---|---|---|
| Original meshes/textures/audio | Original source files with cleared rights | Import/cook recipes and engine bindings |
| Third-party assets | Only assets whose applicable license permits the intended engine/use | Entitlements, acquisition terms, exportability, redistribution |
| Unreal scenes and Blueprints | Approved design intent; our own portable source data when available | Scene/runtime logic, serialization, references, editor behavior |
| Materials, Niagara, Chaos assets | Original source textures or designs when cleared | Material graphs, shaders, effects authoring, simulation integration |
| Motion-matching databases / GASPALS content | Potentially cleared source clips and our own metadata | Per-item rights; database format, feature extraction, search, graph integration |
| Cinemafy, Holofy, Grungify | Our own independently owned logic and concepts where portable | Unreal APIs, compiled assets, editor integration, any third-party/Epic code |
| Narrative-plugin assets | Owned dialogue/script content where export and rights permit | Graph interpretation, conditions, runtime, editor, dependencies |

The engine does **not** ingest `.uasset` or `.umap` files directly. Any later export bridge is a separately approved, read-only migration tool. It cannot be a hidden runtime dependency.

## 9.6 License gate

Fab's current Standard License summary allows assets to be used with compatible tools beyond Unreal. Epic's Content License separately defines UE-Only Content with engine restrictions. These are not a basis for claiming that the entire existing library can move. Terms can differ by asset, acquisition path, date, and separate agreement. [S16, S17]

Keep `asset_provenance.json` records containing item identifier, source, acquisition evidence, applicable license/evidence location, permitted research/runtime/source-sharing uses, reviewer, review date, and status. **Unverified means quarantined.** Self-authored primitives and original shaders are the G1 default.

No blanket claim is made here about MetaHuman, Megascans, Epic sample animation, or any specific purchased pack. Review the actual applicable grant. Keep licensed raw content out of public source packages. These are engineering controls, not legal advice about a particular asset.

# 10. Renderer

## 10.1 Renderer goal and boundaries

Own the shading, visibility, lighting, post-processing, render-resource planning, and frame construction. Use Vulkan as the graphics API, volk as its loader, and VMA for device-memory allocation. VMA does not replace synchronization, resource lifetime, or our render graph. [S2, S3, S4]

The first renderer must be understandable in a frame capture. Advanced techniques are earned by a representative visual requirement and measured budget, not added for a checklist.

## 10.2 Three capability levels

| Level | Required result | Explicit exclusions |
|---|---|---|
| R0 / G1 | Original primitives, camera, depth, flat or simple lit materials, editor viewport, picking, presentation, screenshot | No PBR-parity claim, global illumination, imported animation, ray tracing, virtual geometry |
| R1 / G2 | Static glTF meshes, metallic-roughness PBR, environment lighting, one shadowed directional light, tone mapping, basic antialiasing, useful debug views | No Nanite/Lumen replacement claim; no automatic emissive GI |
| R2 / G4-G5 | Skinned characters, expanded lighting/shadows, decals, effects, temporal stability, conventional LOD/streaming, production-like visual tests | Advanced GI/virtualized geometry remain separate research decisions |

Start R0 with a forward pass. R1 adds environment lighting and shadows. If scene measurements require many local lights, extend to Forward+ light culling rather than simultaneously building forward, deferred, and visibility-buffer renderers.

## 10.3 RHI contract

Expose typed descriptors and generation-checked handles for buffers, textures, samplers, pipelines, command lists, and submission completion. Keep backend objects private.

Initial capabilities include buffer upload, texture upload, render/compute commands when actually needed, indexed drawing, resource copy/readback, render targets, synchronization, and presentation. Unsupported capabilities return typed errors. Do not add fake working methods for ray tracing or mesh shaders.

Start with a single graphics queue and two CPU/GPU frame contexts. Transfer work can execute on the same queue with explicit synchronization. A dedicated transfer queue, async compute, aliasing, and bindless resource allocation are deferred until tested value justifies them.

## 10.4 Resource safety

Track each resource's state and last submission use. Retire resources only after the relevant fence/completion signal. Resize, material changes, shader reload, and viewport closure must not free resources still referenced by in-flight commands.

Separate frame-context indexing from acquired swapchain-image indexing. A two-frame ring does not imply a two-image swapchain. Synchronize acquire/submit/present correctly for the image actually obtained.

Zero-sized/minimized windows suspend rendering safely. Out-of-date/suboptimal swapchain paths recreate dependent targets without using stale framebuffers or descriptors. Device loss produces a diagnostic report and graceful failure; automatic recovery is not a G1 promise.

Vulkan validation and synchronization validation must be part of GPU qualification. Waivers identify the exact message, upstream cause if any, reproduction, and owner; do not suppress every warning to produce a clean screenshot.

## 10.5 Render graph

In R0 a small explicit ordered pass list is sufficient. At R1, evolve it into a graph describing read/write resources and subresource usage. One system owns barrier/layout decisions; do not layer hand-maintained barriers around an unrelated graph that assumes it owns them.

The first graph validates missing producers, incompatible formats/usages, read-before-write, cycles, and invalid lifetime references. It schedules on one queue and keeps resources separately allocated. Transient aliasing and multi-queue scheduling are later optimizations.

Proposed R1 order:

```text
Upload finalized assets
  -> Directional shadow pass
  -> Optional depth prepass (only if measured useful)
  -> Forward opaque / alpha-mask pass into linear HDR
  -> Sky/environment background
  -> Sorted transparent pass
  -> Tone map and selected AA resolve
  -> Editor/game overlays in their intended color space
  -> Present or readback
```

Editor picking may be an ID attachment or a dedicated pass. Prefer an integer entity-ID target using an ephemeral index-to-UUID table for that frame; never truncate a persistent UUID into a collision-prone pixel ID.

## 10.6 Shaders and material model

HLSL is source; DXC emits SPIR-V for the first backend. DXC supports both DXIL and SPIR-V, but the two paths require separate qualification. [S5]

Pin shader target, compiler revision, matrix packing, descriptor-set/binding convention, include roots, optimization flags, and feature defines. Reflection metadata validates CPU/shader resource layouts. Compile shaders during build/cook, not unpredictably during a shipping combat encounter.

Start with a small fixed material family: unlit, metallic-roughness opaque, alpha-mask, simple transparent, and eventually skinned variants. Keep shader permutation counts bounded and reported.

G1 material edits change constants safely. G2 shader reload builds a replacement pipeline off to the side; on failure, retain the prior valid version and show the compiler diagnostic. A shader edit must not reset the entire authoring scene.

## 10.7 Lighting and visual ambition

For R1, use direct lighting, an environment map, and conventional shadow maps. The original First Light hall can look atmospheric through composition, readable materials, deliberate key/fill light, and restrained emissive surfaces. Emission is not illumination unless a lighting solution explicitly accounts for it.

For R2, evaluate reflection probes, baked lighting for static areas, screen-space effects with visible limitations, fog, and later a probe-based or hardware-assisted GI research path. Specify what happens for moving lights, occluders, characters, and disocclusion before describing a feature as dynamic GI.

Virtualized geometry, meshlets, GPU-driven visibility, and hierarchical occlusion can be studied later. They are not interchangeable features, and their absence must not block the early editor/scene proof.

## 10.8 Antialiasing and history

R1 may use a simple spatial AA solution. Temporal AA requires tested motion vectors, jittered camera handling, depth conventions, history rejection, exposure consistency, and animated-mesh motion. Do not ship a blurry history blend under the label of production TAA.

Reset temporal histories after camera cuts, scene swaps, resolution changes, and discontinuous transforms. Transparency and particles need their own policy. Image-quality evaluation includes motion clips, not only stills.

## 10.9 Render extraction and observability

Simulation publishes transforms, mesh/material IDs, animation-pose handles, lights, cameras, and bounds into a render snapshot. Renderer may cache GPU state, but cannot become the owner of gameplay transforms.

Required diagnostics grow with each feature: frame number, CPU frame time, GPU timing when supported, draw count, visible instances, triangle counts, memory allocation/residency, shader/pipeline counts, asset upload queues, and current backend/device identity.

Debug views include normals, albedo, roughness, metallic, depth, shadow coverage, wireframe when supported, and entity IDs as their underlying features arrive.

# 11. Editor and transactional authoring

## 11.1 Native editor, shared services

The editor is an original executable built with Dear ImGui for immediate tool UI. Dear ImGui explicitly targets tools and has limitations in full internationalization and accessibility; use it knowingly, not as evidence that game UI requirements are solved. [S7]

G1 panels: scene hierarchy, viewport, inspector, project/scene controls, command history, diagnostics, and a small performance display. Transform entry fields are required. A polished gizmo is optional until the basic edit contract is correct.

Editor chrome should be neutral dark graphite with restrained bronze highlights, readable text, and clear selection/warning states. Do not hard-code faction colors into usability-critical diagnostics. This is a workbench, not a game HUD.

## 11.2 One authoring path

GUI, CLI, and agents call the same command services. They do not separately mutate JSON or ECS fields.

An edit command identifies project, scene UUID, base revision, operation, entity UUID, and typed fields. An edit transaction validates the entire batch against a staged copy, including cross-operation effects, before commit. A drag interaction previews locally and commits a coalesced operation rather than hundreds of unrelated undo entries.

First commands: create entity, rename, set transform, set supported material parameters, delete an unreferenced entity, and undo/redo. The included batch schema covers the five forward-edit operation kinds. Undo/redo are separate history-service calls with the same scene lock, revision checks, and journaling rules; they are not mixed into a forward-edit batch. Import, bake, render, and package are jobs, not ordinary reversible scene mutations.

## 11.3 Transactions and revision rules

Reject stale `base_revision` values. Return current revision and conflict details; never silently overwrite newer human edits. A successful non-empty mutation increments scene revision once, regardless of operation count. Undo and redo also produce new revisions rather than rewinding the revision counter.

An idempotency key is scoped to scene/project and actor. Bind it to the request-content hash. Retrying the same successful request returns the recorded result; reusing the key with different content fails. A reused expired key cannot silently become a new action: return an explicit unknown/expired status requiring inspection or a new request.

A batch failure produces no authored scene change. The transaction contract must specify deletion dependencies, operation order, and reference resolution. Durable exactly-once behavior needs a commit journal or generation record tying request identity to the scene commit; two unrelated JSON writes are not sufficient.

## 11.4 Play mode

Play creates a separate runtime-world instance from the validated authoring scene. Simulation, physics, temporary effects, and spawned entities cannot mutate the authoring document.

Stopping play discards the runtime world. A future 'apply selected changes' feature requires an explicit diff, validation, and new transaction. G1 has no automatic write-back.

The editor camera and play camera have independent input focus. Escape releases capture without terminating a save. Minimize, focus loss, pause, single-step, and stop/play loops are tested.

## 11.5 Recovery

Autosave writes a separate recoverable snapshot and journal marker, not destructive replacement of the user's last explicit save. After a crash, compare committed document, journal state, and recovery snapshot. Offer recovery with a visible diff.

G1 may implement a simpler single-scene backup plus verified atomic save; full session recovery is a later gate. Reports must name which durability guarantees actually exist.

# 12. Physics and character movement

## 12.1 Responsibilities

Jolt provides collision and physics capabilities. The engine owns world lifecycle, collision-layer policy, physics-to-entity mapping, character integration, gameplay queries, event delivery, and debugging. [S10]

Use one physics world per play/runtime world. Character motion is initially kinematic through a tested virtual-character/controller integration, not by teleporting a dynamic rigid body each tick. Dynamic props have a different ownership path.

Collision categories start small: world static, world dynamic, character, projectile/query, trigger, and editor-only. Query filters and simulation-pair filters are distinct and tested. Queries can exclude the instigator without accidentally excluding all characters.

## 12.2 One movement authority

`MovementIntent` can be requested by player input, AI navigation, a scripted action, or root motion. A `MovementAuthority` resolver selects the valid owner for the current state. The character motor consumes the chosen intent and applies collision-resolved movement once.

Root motion is a displacement request, not permission to bypass collision. Report the difference between requested and resolved displacement. Animation warping and foot-placement corrections respond to that difference without independently teleporting the actor.

Physics controls ragdolls and dynamic bodies; gameplay controls the character motor; recorded cinematics may explicitly take control in a non-gameplay playback mode. Authority changes are state transitions with validation and cleanup, not competing writes.

## 12.3 Locomotion qualification

G3 requires a capsule character that walks, turns, stops, and respects world collision. A third-person camera follows without clipping into walls. Required fixtures: flat floor, slope, step, narrow passage, low ceiling, moving prop, wall corner, spawn overlap, fall/reset, and frame-rate variation.

Stairs, dynamic moving platforms, crouch-clearance tests, jump buffering, dodge, and advanced traversal are separate features. A navigation path is not proof that the character can physically traverse it.

High-speed projectiles and melee use appropriate continuous/swept queries. Do not assume that a single instantaneous overlap catches fast motion. Contact callback delivery is buffered into the simulation owner's event processing and must not directly edit the ECS from a physics worker.

# 13. Animation and motion matching

## 13.1 First animation capability

G3 starts with skeleton import, bind-pose validation, clip sampling, hierarchical pose composition, GPU skinning, simple transitions, and named animation events. Blend two correct clips before building a node-graph editor.

Test local/global transforms, inverse bind matrices, normalized skin weights, joint-index bounds, clip loop boundaries, zero-length clips, nonuniform time samples, interpolation modes, and missing optional channels. Rendering a static bind pose does not satisfy animated-character qualification.

## 13.2 Animation responsibilities

**Animation evaluation** samples and blends poses.

**Action selection** decides what the character is attempting, from gameplay or cinematic direction.

**Motion matching** chooses an appropriate segment from a cleared animation database, using trajectory/pose features and declared constraints.

**Character motor** decides where the collision-constrained actor actually moves.

**IK/contact adjustment** improves the pose relative to the resolved environment.

**Cinematic director** schedules authored intentions and shot structure.

None of these responsibilities is automatically performed by importing a motion library. Epic-specific databases and runtime implementations are not portable merely because source animations can be obtained.

## 13.3 Motion-matching research gate

Do not implement motion matching before the clip player and motor pass their tests. Begin the later research gate with a compact, cleared locomotion dataset: idle, forward/backward movement, strafes, turns, starts, and stops.

Offline processing extracts joint positions/velocities in a declared root-local frame, future trajectory positions/headings, contact labels, gait/action metadata, and segment validity. Dataset version and extraction settings belong in the database key.

A first search implementation can be exhaustive for a small dataset. Measure it before adopting a tree/index. Normalize feature dimensions, expose weights, enforce hard constraints before soft costs, and penalize unnecessary discontinuities.

Retain pose/velocity continuity through blending or inertialization only after each is independently tested. Include negative cases: no valid segment, changed speed, sharp turn, obstacle, start/stop, missing action, and contact mismatch.

Do not promise neural animation, model training, or cross-skeleton automatic retargeting in this gate. Those are separate research tracks with data rights, evaluation, and maintenance costs.

## 13.4 Root motion, hit windows, and interruptions

Each action specifies whether root motion is extracted, ignored, or used only for presentation. Loop wrap and playback-rate changes must preserve displacement consistency.

Gameplay events carry stable action instance and marker IDs. If a marker interval is crossed twice through a loop or replay, the owning system applies its explicit repeat policy. Combat effects cannot be fired from both a pose sampler and a gameplay timer for the same event.

Interrupts cancel the action's future gameplay windows and release movement ownership. Presentation may blend out, but cancelled attacks cannot continue dealing damage because a montage-like clip is still visually finishing.

# 14. Gameplay foundation and combat

## 14.1 Generic services, game-owned rules

Implement a small **Ability and Attribute Framework**, inspired by the needs of action games but independently designed. It is not a port or reimplementation-by-copying of Unreal GAS. Do not reproduce GAS's entire prediction, replication, tooling, or effect surface in the first combat proof.

Engine services provide typed attributes, hierarchical tags, ability definitions, activation state, costs, cooldowns, timed effects, effect ownership, and semantic events. Game modules define names, numbers, faction rules, weapon behavior, damage policy, resource generation, and narrative consequences.

Tags are interned runtime IDs generated from a collision-checked registry. Persist canonical tag strings or versioned IDs with a registry fingerprint. Test exact and parent/child matching separately.

## 14.2 Ability lifecycle

```text
Request
 -> Validate owner / target / state / cooldown / resources
 -> Reserve or commit declared costs exactly once
 -> Start action instance
 -> Advance phase windows
 -> Resolve authorized effects
 -> Complete or interrupt
 -> Cleanup ownership / subscriptions / temporary state
```

Each activation has an instance UUID or equivalent collision-safe runtime identifier. It defines activation requirements, cost timing, allowed cancellation windows, target rules, animation/action reference, effect windows, cooldown start policy, and cleanup behavior.

An activation failure must not spend resources, play a successful action, or start an unrelated cooldown. An interrupted action follows an explicit refund policy. The initial rule is no refund after a successful activation unless that ability definition says otherwise.

## 14.3 Attribute/effect semantics

The framework clamps bounded attributes and distinguishes base values from evaluated values. A proposed modifier pipeline is:

```text
value = clamp((base + sum(flat_add)) * product(multiplier), min, max)
```

Overrides, if later needed, have a declared precedence and cannot be mixed ambiguously with ordinary modifiers. Effect stacking policies are named: independent instances, refresh duration, replace by priority, or capped additive stacks. Every combination implemented needs tests.

Changing a maximum attribute uses a game-selected policy: clamp current, preserve absolute deficit, or preserve percentage. There is no silent universal choice.

Effect expiration and cancellation occur on simulation time. Presentation subscriptions cannot become a second health or resource authority.

## 14.4 Damage and hit resolution

A hit result contains attacker/target IDs, attack instance, hit-window ID, contact/query information, damage payload, and resolved outcome. The initial per-window hit ledger prevents repeated damage to the same target unless the attack explicitly allows repeat hits.

For the sandbox, damage resolves through immunity, game-defined modifiers, shield absorption, health spillover, optional poise/effects, and one death transition. Damage routing belongs to one service; the projectile and the target do not both subtract health.

Companion down/revive, critical hits, armor formulas, weak points, deflection, finishers, corruption, and Echo-generation policies are **not** implied by this primitive pipeline. Add only what the selected gate tests.

## 14.5 First combat demonstration

Create an original **Test Operative**, not a claim that Tarrik's or Selene's current kit is ported. Use simple geometry until cleared character content is available.

The G3 loop includes a player, three enemies, one hitscan weapon, one melee action, damage feedback, death/despawn or disable, reset/checkpoint, a minimal HUD, and an executable package. Ability costs and cooldowns must run through the shared framework.

All tuning is in `sandbox` data. Proposed **test-only** values:

| Field | Value | Purpose |
|---|---|---|
| Health | 100 | Easy-to-verify survivability arithmetic |
| Shield | 50 | Verifies shield-to-health spillover |
| Hitscan damage | 20 | Three hits leave shield 0 and health 90 |
| Melee damage | 25 | Distinct query/action fixture |
| Ability resource | 100 maximum, 20 cost | Verifies atomic activation and insufficient-resource rejection |
| Cooldown | 30 simulation ticks | Verifies fixed-tick scheduling at the default 60 Hz |

These are not Origins balance recommendations. Death, regeneration, ammo reserve, poise reset, and revival defaults from the old game TDD must not sneak into engine behavior.

## 14.6 Game-module seams

Future modules may implement `OriginsCombat`, `ReachArenaCombat`, `SettlementSimulation`, and `MMOServerRules`. They share serialization, entity services, time, resource/effect primitives, and observability where appropriate.

Do not force turn-based or queued squad combat through a third-person controller. Do not require an MMO server to instantiate a renderer, audio device, cinematic camera, or local input subsystem.

# 15. AI, navigation, and companions

## 15.1 Ownership split

Recast builds navigation data; Detour queries paths and supports navigation-related facilities. Behavior selection, sensing, threat, action execution, and character movement remain engine/game responsibilities. [S11]

The first AI uses a small explicit finite-state machine: idle, acquire target, request path, approach, attack, recover, dead. A general StateTree/behavior-graph editor is deferred.

## 15.2 Perception and path requests

Sight checks include range, field of view, and line of sight. Hearing is event-driven. Awareness and memory are explicit gameplay data, not renderer visibility. An offscreen enemy is not necessarily blind.

Requests carry world generation, entity generation, destination, and cancellation token. Results from an old world or a destroyed agent are discarded. An unloaded navigation cell returns unavailable/partial status instead of an invented complete route.

The character motor consumes steering intent. Detour or a behavior node must not write the transform while the motor also moves it.

## 15.3 Budgets and tests

Stagger perception and repath work. Record query count, duration, queue backlog, failed paths, and recovery attempts. Keep algorithmic correctness separate from crowd-scale performance claims.

Tests include unreachable target, stale path, blocked corridor, invalid spawn, target death mid-attack, line-of-sight loss, scene unload, and re-entered encounter. Teleport-to-safety is a logged recovery policy, not a way to hide routine navigation bugs.

A future companion layer adds follow positioning, line-of-fire avoidance, support actions, narrative participation, and explicit recovery. Approval affects game decisions, not core pathfinding correctness.

# 16. Narrative and native Cinemafy

## 16.1 Narrative data

A future narrative module stores dialogue graphs, speaker IDs, localization keys, choices, conditions, actions, quest state, relationship variables, and story flags. Actual manuscript content remains a game-owned asset.

Use a typed condition/action registry. No arbitrary shell, file access, or unrestricted code evaluation from a dialogue node. Validators catch missing speakers, broken edges, unreachable required nodes, unintended loops, and missing outcome actions.

Persistent story facts, mutable current state, and derived presentation state are different things. For example, 'door was unlocked' can be historical, while 'door is open now' is mutable. Do not encode every changing property as an immutable Boolean flag.

## 16.2 Cinemafy as a native engine tool

The long-term goal is to select a character, place spatial/action intentions, and author a directed performance. This is a new native implementation informed by the desired workflow, not a claim that the existing Unreal plugin can run unchanged.

The core data boundary is:

```text
Authored beat
  -> Validated/resolved action plan
  -> Live execution under engine authorities
  -> Recorded result / baked take
```

An authored beat describes intent, participants, target marker, relative timing, action identifier, constraints, and optional language annotation. A resolver selects a known capability and produces a plan. Execution reports the actual result: completed, interrupted, unreachable, unavailable animation, failed contact, or timed out.

A one-word prompt is not itself a movement or animation capability. Language assistance may suggest a plan, but unknown actions remain unresolved and visible. No network/model access is required for basic directing, playback, or runtime execution.

## 16.3 Division of responsibilities

| System | Owns | Must not own |
|---|---|---|
| Director | Narrative intent, relative timing, shot schedule | Raw character movement or damage |
| Action resolver | Capability lookup, target validation, execution plan | Invented unsupported animation |
| Navigation | Feasible route/steering suggestion | Independent transform writes |
| Motion matching | Locomotion segment selection | Deciding the narrative action or destination |
| Character motor | Collision-resolved movement | Camera composition |
| Animation/IK | Pose, blending, contact correction within limits | Unbounded root teleportation |
| Camera system | Framing, lens, shot transitions | Character position changes to fix framing |
| Recorder | Sampled transforms/poses/events and timing | Re-solving the scene differently during playback |

## 16.4 Timeline and takes

One master sequence clock controls authored timing. Store rational frame rates and integer time units/ticks suitable for sample mapping, not accumulating floating-point frame deltas indefinitely.

A recorded take includes engine/schema versions, scene revision, asset hashes, participants, actual transforms, animation references or baked pose data, camera states, relevant events, and time mapping. Audio references and offsets are part of the take; rendering does not repeatedly retrigger them while scrubbing.

Separate a live simulation preview from a recorded playback mode. Baking should preserve the observed result, not rerun AI navigation and hope to get the same performance. Scrubbing a baked take must not reapply damage, spawn enemies, or mutate persistent quest state.

Regeneration creates a new take or layer. Preserve manual camera keys, approved animation edits, and timing locks unless the user explicitly targets them. Store the generation inputs and a diff.

## 16.5 Runtime cinematics

Use a cinematic session that acquires input/camera/actor-control permissions and releases them reliably on completion, skip, cancellation, load, or error.

Separate presentation events from final gameplay outcomes. On skip, apply an idempotent final-outcome action list and intended end transforms. Do not fast-forward through every explosion, footstep, projectile, and dialogue event and replay their side effects.

When a required streamed area is unavailable, preload or enter an explicit transition state. Never teleport a player into unloaded collision. Restore gameplay only after the post-scene world state is valid.

## 16.6 G4 cinematic proof

Author a short original encounter: the Test Operative approaches a marker, turns toward a second actor, plays a simple action, changes camera shot, and returns to player control. The exact duration is a test parameter, not a content target.

Demonstrate normal playback, skip, replay, baked scrubbing, failed/unreachable action, interrupted preview, and recovery. The camera must not move the character to make a shot look correct. Facial performance, lip sync, voice synthesis, and production dialogue tools remain distinct later requirements.

# 17. Input, cameras, audio, effects, and game UI

## 17.1 Input

SDL3 collects platform events; an engine action layer maps keyboard, mouse, and gamepad input to named actions. Gameplay sees actions and values, not hard-coded OS key codes. [S1]

Support focus changes, held/pressed/released semantics, dead zones, cursor capture, and input-context priority. An inspector text field cannot simultaneously trigger gameplay movement. Rebinding and persistent device preferences follow after the basic action layer is tested.

The runtime has no editor shortcuts. Offline replay supplies recorded action inputs, not simulated mouse clicks.

## 17.2 Cameras

Camera modes include editor orbit/free camera, runtime third-person follow, and cinematic playback. Only one system chooses the final active camera, with explicit blends and cuts.

Third-person follow uses collision-aware placement and stable target smoothing. Camera smoothing runs in presentation time but does not change authoritative character motion. FOV, invert axes, sensitivity, and shake options are game-facing configuration.

Camera cuts reset temporal histories. A fallback camera is clearly named and only used when the scene's camera is missing according to an explicit policy.

## 17.3 Audio

Integrate miniaudio behind an engine-owned audio service. Begin with WAV playback, master/music/dialogue/effects buses, position/attenuation, and bounded simultaneous voices. miniaudio is an audio foundation, not a complete cinematic authoring system. [S12]

Audio commands are queued. Do not load/decode files or block on ECS state from a realtime callback. Missing optional sound fails gracefully. Dialogue subtitles and gameplay cannot wait forever on audio completion after a device loss.

Qualification includes device absence, mute/volume persistence, scene stop, repeated playback, overlapping voices, and no double-triggering during pause/scrub. Advanced acoustics, occlusion, and platform audio certification are deferred.

## 17.4 Effects

The first effects layer supports bounded simple particles, impact flashes, and decals when the renderer supports them. A typed effect event describes location, direction, material/surface category, lifetime, and deterministic seed where useful.

Effects never deal authoritative damage through a rendering callback. Separate visual projectile trails from collision/projectile simulation. Pooling is introduced when allocation measurements justify it.

Native Holofy/Grungify-style tools may follow as custom material/effect utilities. Their names are not proof that Unreal shader graphs or plugin bindings have migrated.

## 17.5 Game UI

The debug HUD may use ImGui during G3. A production game UI needs its own input focus/navigation, layouts, text shaping, localization, scalable fonts, safe-area support, resolution/DPI handling, and accessibility work. Do not mistake an attractive debug overlay for a shippable mobile or controller UI. [S7]

Plan a lightweight retained game-UI tree driven by observable game state. Attributes/events update widgets; widgets do not own gameplay state. Avoid per-frame string formatting and layout work when data did not change.

For Sovereign Call, themes are data: Dominion bronze/oxblood/amber, Reformation graphite/muted blue/cool light, Remnant white/gold, Eclipse dark/violet. Provide shape/text cues in addition to color.

# 18. Saves, checkpoints, and streaming

## 18.1 Authoring documents versus save games

Scene documents describe authored content. Save games store game-owned persistent state. A runtime checkpoint stores enough validated state to restart an encounter/mission at a known safe point. They are not interchangeable serialized ECS memory dumps.

A save header includes schema version, engine/game version, content-manifest hash or compatibility identity, active region/checkpoint, and integrity metadata. Persist stable IDs and typed values, not pointers, GPU handles, or raw component layouts.

## 18.2 Restore pipeline

Validate the save before mutating the current live world. Build or stage the target world, resolve required content, restore persistent entities and game state, reestablish physics/navigation/runtime bindings, then publish it as active. Failed restoration leaves the prior usable state intact or returns to a safe menu.

Gameplay cooldowns/effects must define save behavior explicitly. Temporary render/audio state is generally reconstructed. Checkpoint reset cannot duplicate quest rewards, kill rewards, or one-shot outcomes.

G3 needs one small sandbox checkpoint/reset. Versioned long-lived saves, migration tools, and corrupt-file recovery arrive with G5.

## 18.3 Streaming

Begin with explicit scene loading. G5 introduces authored cells/chunks with dependency manifests, navigation/collision membership, asset residency budgets, and activation states.

Proposed cell lifecycle:

```text
Unloaded -> Requested -> Loading -> Validated -> Ready -> Active
        -> Deactivating -> Retiring -> Unloaded
```

Render visibility, gameplay activation, collision readiness, and navigation readiness are related but distinct. A visually present floor is not automatically safe to spawn on.

Unload waits for gameplay references, asynchronous jobs, and GPU use to retire. Persistent entity IDs survive cell reload according to policy. Cross-cell references are handles that can explicitly report unavailable, not dangling pointers.

A cinematic/teleport preloads its destination and waits for required readiness. Streaming tests include rapid backtracking, failed asset load, stale navigation result, cancel/retry, a character crossing a cell boundary, and restoration after a checkpoint.

# 19. Agent interface and security

## 19.1 API-first, not model-dependent

The engine exposes typed commands and queries that humans, scripts, and agents can use. It does not contain a mandatory LLM runtime. Natural-language interpretation is an optional external client, never the authority for validation.

G1 uses local JSON command batches through `sovereign-cli` plus the same in-process command service in the editor. Offline CLI edits require an exclusive scene lock and fail while another writer owns it. Do not edit the same scene file from an agent and editor concurrently by bypassing revision checks.

Live IPC is introduced only when needed. Prefer a local transport with OS access control and authenticated session ownership. A loopback TCP port is not automatically trusted. No public listener, unauthenticated RPC, or unrestricted shell endpoint.

## 19.2 Command batch contract

The included `schemas/command_batch.schema.json` describes the initial wire contract. A batch contains protocol version, request ID, actor label, scene ID, base revision, idempotency key, dry-run flag, and bounded typed operations.

Supported first operations:

- `entity.create` with an explicit stable entity ID and initial components.
- `entity.rename`.
- `transform.set` with complete position/rotation/scale values.
- `material.set` with supported material fields.
- `entity.delete` with dependency validation.

Each operation carries its own target. There is no ambiguous batch-wide target that accidentally changes the wrong entity.

`dry_run` returns the proposed diff and diagnostics without incrementing revision, writing files, or reserving an idempotency key as successfully committed. The same validation runs again during real commit against the then-current revision.

## 19.3 Results and errors

A response includes request ID, status, scene revision before/after, operation results, validation errors, diff summary, and undo/commit token when applicable. Error codes distinguish schema error, stale revision, unknown entity, missing dependency, unsupported component, permission denied, lock unavailable, and I/O failure.

Successful scene mutation is not the same as a successful render. Long operations such as cook/render/package return job identity and observable job status. They execute outside a rollbackable world transaction and publish artifacts atomically on success.

## 19.4 Safety controls

Limit operations, payload bytes, file sizes, nesting, job duration, and output location. Resolve project-relative paths after normalization and reject escapes through `..`, symlinks, or absolute paths according to an explicit policy. Treat imported asset text and external metadata as data, not instructions.

Agents receive scoped authority: project, scene, operation set, and asset write areas. Sensitive operations such as production export, package publication, dependency changes, or deletion of unrelated assets are not implied by a scene-edit permission.

Logs redact credentials and sensitive local paths before sharing. No telemetry or asset upload occurs by default. Dependency fetching is a separately recorded build action, not a hidden runtime behavior.

## 19.5 Testability

A scene edit completed through the GUI and the same edit completed through the CLI must produce equivalent authored state. Test duplicate delivery, conflicting human edits, retry after failure, reuse of a key with changed content, invalid batch rollback, restart after commit, and malformed/untrusted inputs.

The JSON schemas are contract examples. They do not implement runtime validation, persistence, transactions, or security. Caelis must implement and test those guarantees in the engine.

# 20. Platform profiles and networking boundaries

## 20.1 Desktop first, not desktop-only forever

Initial qualification targets Windows x64 game delivery, with Linux x64 as a practical development/test host where available. Record platform qualification individually. Passing Linux tests does not close a Windows build gate.

RHI and platform boundaries should permit later D3D12/Metal work without exposing Vulkan in game logic. That is an architectural allowance, not a committed backend implementation.

Mobile requires independent shader/material budgets, touch/UI design, texture formats, packaging/signing, memory-pressure handling, suspend/resume, device thermal testing, and actual iOS/Android runtime tests. Vulkan on a desktop does not prove mobile readiness. iOS also needs a qualified rendering/platform route rather than an assumed desktop port.

Consoles are outside current scope. No platform SDK access or certification capability is assumed.

## 20.2 Data profiles

A project manifest selects `DesktopCinematic`, `ArenaMobile`, or `DedicatedServer` profile when those profiles actually exist. Each defines features, memory/performance budgets, content limits, and cook options.

No profile should silently use the most expensive desktop material or VFX feature. Cook validators reject unsupported content rather than relying on runtime failure.

## 20.3 Multiplayer and MMO

Design input/action commands so a future authoritative server can validate them. Keep player input, world state, effects, and presentation separate. Use stable IDs and bounded state serialization. These decisions help networking but do not make the engine network-ready.

Defer transport selection, replication protocol, prediction/reconciliation, interest management, account systems, persistence services, shard orchestration, commerce, anti-cheat, and live operations to an MMO TDD with its own tests.

A dedicated server must build without renderer/editor/audio dependencies. Physics repeatability should be qualified for the intended role; do not assume deterministic lockstep is the right architecture for an action MMO.

The first multiplayer proof, when explicitly authorized, is a small authoritative client/server interaction with loss/latency/reconnect tests. It is not thousands of bots in a single local scene.

# 21. Reference scenes and performance qualification

## 21.1 Test scenes

**S0: Axis Lab.** Original boxes/planes and asymmetric axis markers, camera, depth overlaps, parent/child transforms, and a material swatch. Used for math, projection, picking, serialization, and GPU startup. This must not depend on downloaded art.

**S1: First Light.** A compact original ceremonial sci-fi hall assembled from primitives. Suggested layout: roughly 30 by 18 meters, repeated columns, a central walkway, and a raised platform. Use bronze, deep red, charcoal, and restrained emissive accents as material data. The setting expresses the Dominion-inspired visual direction without importing any production scene or asset.

G1 uses simple shading. G2 evolves the same hall with approved static GLB assets, PBR materials, environment lighting, and directional shadows. Maintain versioned scene manifests so a performance comparison does not quietly change content.

**S2: Combat Yard.** A bounded arena with a player, three enemies, cover/obstacles, one checkpoint, and a minimal combat HUD. This is the G3 test, not an adaptation of a particular Origins mission.

**S3: Director Room.** Two actors, a reachable marker, an intentionally unreachable marker, two cameras, one action clip, and a short event timeline. Used for G4 live performance, bake, scrubbing, skip, and restoration.

**S4: Reach Arena.** Later ten-unit 5v5 sample using game-owned rules. It is a bounded desktop qualification fixture before any mobile-production claim.

## 21.2 Budget policy

Performance numbers here are **provisional engineering targets**, not measured results or promises. G0 records a real reference machine; G1 establishes the first baseline. Matthew must approve later target changes, and reports must show before/after results instead of redefining success retroactively.

| Profile / fixture | Provisional limits or targets |
|---|---|
| G1 S1 content | Up to 100 visible primitive instances, up to 100,000 submitted triangles, small constant-material set |
| G2 S1 content | Up to 500 visible static instances, about 1 million submitted triangles, up to 64 material instances; texture residency measured and capped in the fixture |
| Desktop qualification resolution | 1920 x 1080, fixed internal resolution, no frame generation, quality settings recorded |
| Desktop frame target | Median steady-state frame time at or below 16.7 ms; p95 at or below 20 ms; p99 at or below 33.3 ms on the recorded reference machine |
| Investigation thresholds | CPU frame work above 6 ms or GPU work above 12 ms prompts profiling; these are not a claim that their sum equals frame time |
| Stability | No reproducible crash, corruption, invalid memory access, or unbounded growth during the defined test suite |
| Authoring integrity | Invalid batch leaves scene state/revision unchanged; save/reopen preserves authored meaning |

These budgets apply to these fixtures. Meeting them says nothing about a full Origins mission, mobile thermals, or MMO capacity.

## 21.3 Measurement procedure

Record engine commit, dependency lock hash, scene/content hashes, build configuration, compiler, operating system, GPU/driver, CPU, RAM/VRAM, display mode, renderer settings, and device features.

Run the same camera path and workload, warm caches deliberately, then collect several repeated samples. Report cold startup/first-load behavior separately from warmed frame timing. Include p50/p95/p99, CPU/GPU breakdown, memory high-water marks, and spikes. Do not present only an average FPS.

Use an uncapped or explicitly controlled timing mode for performance runs and a presentation-stability run with the selected vsync mode. State which measurement includes vsync waits. A frame capture assists diagnosis but can distort timing, so do not use instrumented capture timings as the only performance evidence.

## 21.4 Visual review

A technically correct renderer can still fail the desired art direction. Capture fixed-camera images and a short motion clip from the named scene revision. Matthew reviews readability, material response, silhouette, shadow quality, camera comfort, temporal stability, and overall mood.

Image regression uses tolerances and explicit baselines on pinned hardware. Do not require bitwise identical images across all vendors. Cross-vendor comparisons should find structural defects, not fail on every harmless floating-point difference.

## 21.5 Stability and adversarial tests

Run at least 50 open/save/close cycles for the small fixture, 50 play/stop cycles once play exists, repeated resize/minimize/restore, invalid JSON/asset cases, stale commands, duplicate requests, failed write injection, and shutdown during an unfinished job.

Use sanitizers where supported, compiler warnings, and graphics validation. A sanitizer-unavailable platform is named as such. Put the required tests in CI when the runner supports them, and keep GPU tests separate from headless CI status.

# 22. Implementation gates

## 22.1 Gate sequence

```text
G0 Reproducible foundation
 -> G1 First Light: edit/save/run
 -> G2 Assets, PBR, and cook/package
 -> G3 Combat Yard
 -> G4 Native directing and animation research
 -> G5 Production reliability and representative scale
 -> G6 Per-game migration decision
```

The first handoff authorizes **G0 and G1**. Later sections define the destination without authorizing Caelis to build every subsystem in one uninterrupted change.

A blocked GPU gate does not prohibit productive headless work. It does prohibit marking the whole gate complete or skipping to unrelated gameplay features.

## G0: Reproducible foundation

**Goal:** Know where we are building and make the smallest independent targets compile.

**Deliverables:** Host inventory, isolated repository, build presets, dependency lock, notices, a working CLI/version command, unit test target, log/error conventions, schema fixtures, and decision records for the selected toolchain/backend.

**Acceptance:** A clean configure/build/test procedure works on the recorded host; headless targets do not require a display or Vulkan initialization; dependencies are pinned; no production paths changed; no Unreal headers, libraries, or tools are required.

**Proof:** `g0-report.md`, configure/build/test logs, dependency/license manifest, and commit identifier. Record untested platforms without inference.

**Exclude:** Renderer architecture embellishment, engine logo/website, combat, physics, scripting, networking, and a plugin marketplace.

## G1: First Light

**Goal:** Create, edit, persist, and run a real original scene independently of Unreal.

**Work order:**

1. Implement math/scene identity and strict validation, with round-trip tests.
2. Implement command validation, staged mutation, revision conflicts, atomic scene commit, and idempotency behavior.
3. Expose offline scene validation and command batches through CLI with writer locking.
4. Open SDL window and initialize the real Vulkan backend with diagnostic output.
5. Render the S0 fixture with camera, depth, and original primitives.
6. Add the editor viewport, hierarchy, inspector, selection, and command history.
7. Implement undo/redo, save/reopen, and isolated play/stop.
8. Load the authored scene in a separate `sovereign-runtime` executable.
9. Assemble S1 First Light from the supported original primitives.
10. Produce GPU evidence and run the G1 validation matrix.

**Acceptance:** Native rendering on a named physical GPU; correct transform/picking/depth behavior; create/change/delete/undo/redo; save/reopen; CLI edit equivalent to GUI edit; invalid batch rollback; stale-revision rejection; duplicate-request handling; runtime without editor UI dependencies; safe play-world isolation; resize/shutdown tests; no unapproved production changes.

**Proof:** Editor image, runtime image, scene before/after/diff, command request/result, test logs, GPU validation log, reproducible commands, and a status report. Screenshot files must identify their real capture source.

**Exclude:** Imported skinned assets, Jolt/Detour integration, motion matching, native Cinemafy, PBR parity, GI, virtual geometry, MMO, mobile backend, public distribution.

**Stop/report boundary:** Report what is actually verified and the next proposed task. Do not proceed to G2 on the strength of a pretty screenshot alone.

## G2: Content pipeline and static visual quality

**Goal:** Prove the engine handles real cleared source assets, interprets materials, and packages cooked content.

**Deliverables:** Narrow GLB importer with explicit unsupported-feature errors; asset registry; incremental cook/cache; static metallic-roughness material path; environment lighting; one directional shadow; diagnostics; packaged runtime containing cooked assets and required notices.

**Acceptance:** Asset/material/axis fixtures pass; source-change invalidation works; failed reimport retains the previous valid artifact; required missing/uncleared content blocks cook; runtime runs from a clean output directory without editor or source-asset paths; PBR hall reviewed in motion; shader reload failures preserve valid rendering.

**Proof:** Cook manifests and hashes, import diagnostics, before/after images, representative frame metrics, package contents, clean-machine/directory test instructions and actual run results.

**Exclude:** General material graph editor, USD/FBX importer, automatic Unreal conversion, full shader-feature parity.

## G3: Combat Yard

**Goal:** A small original playable game loop on the new runtime.

**Deliverables:** Character motor, Jolt collision/physics integration, clip player and skinning, basic navigation/AI, ability/attribute primitives, hitscan and melee, basic audio/HUD, checkpoint/reset.

**Acceptance:** Player and three enemies complete the encounter/reset loop; collision and animation are consistent; one movement authority; no repeated hit per attack window; deterministic test arithmetic; cooldown/resource rejection; stale AI work cannot modify dead or unloaded entities; play/stop does not alter authored content; standalone package works.

**Proof:** Gameplay recording, named scenarios, metrics, native/headless/GPU test results, unresolved issues, game-data fixture version.

**Exclude:** Production protagonist kits, full inventory/economy, branching campaign, advanced facial animation, online services.

## G4: Native directing and motion-matching research

**Goal:** Prove the workflow that makes this engine personally valuable, not merely different.

**G4A, directing:** Authored beats, validated capabilities, execution reporting, camera timeline, non-destructive takes, bake/playback/scrub, skip/final-state reconciliation, reliable return to gameplay.

**G4B, motion matching:** A cleared small locomotion database, documented features/weights, constrained search, transition behavior, and observable motor/contact interaction. G4B can be evaluated separately from G4A.

**Acceptance:** S3 succeeds and fails honestly; camera never drives character movement; unreachable/unsupported actions report their cause; baked playback reconstructs the recorded result within declared tolerances; scrubbing has no gameplay side effects; manual take edits survive regeneration; motion matching improves a named locomotion case without hiding regressions.

**Proof:** Live and baked recordings, timing/contact comparisons, take diff, capability failures, dataset provenance, performance and search-debug data.

**Exclude:** Automatic generation of arbitrary acting, full facial performance, cloning Epic databases, one-prompt production cutscenes without review.

## G5: Production reliability and representative scale

**Goal:** Establish that the engine can survive ongoing content development, not just demonstrations.

**Deliverables:** Versioned saves/migrations, crash recovery, stronger content validation, prefabs/overrides as needed, world streaming, asset residency budgets, profiling, game UI fundamentals, multi-machine packaging, regression infrastructure, representative visual/animation quality.

**Acceptance:** A substantially representative mini-mission or arena survives content revisions, save/load, streaming, restart, error recovery, and repeated builds. Qualified platforms and hardware configurations are explicit. Known limitations and workarounds are documented and acceptable to Matthew.

**Proof:** Repeatable production workflow comparison, memory/frame traces, reliability results, content-authoring recordings, package/install tests, and current risk register.

**Exclude:** Calling the engine 'Unreal-equivalent' because the sample works.

## G6: Per-game adoption decision

**Goal:** Decide whether any specific game benefits from adoption.

Recreate an approved representative slice in a separate project, never over the production original. Compare content creation, image/motion quality, gameplay reliability, iteration time, memory/performance, deployment, and ongoing maintenance. Migration of one bounded game does not authorize migration of Origins, the city builder, or the MMO.

Matthew makes the adoption decision. A successful research engine may remain a tool/research project indefinitely without being a failed experiment.

# 23. Engineering effort and working method

## 23.1 Plan by uncertainty, not generated code volume

This is self-funded work. No external team, funding, or paid infrastructure is assumed. Additional agent availability increases implementation capacity, but does not automatically provide physical-GPU testing, animation data rights, human visual review, or correct integration.

The first estimates should be counts of small verified work items and unresolved risks, not a promised multi-year calendar. Re-estimate after G1 and G3 using actual build/test cycles, regressions, and the time needed to produce an accepted result.

| Work package | Relative effort / uncertainty | Why |
|---|---|---|
| G0 portable core and reproducible build | Moderate / host-dependent | Toolchain/dependency setup and clear boundaries |
| G1 scene/commands/editor/runtime | High / bounded | Several interacting products, persistence and GPU lifecycle |
| G2 import/cook/PBR | High / high | Malformed content, material correctness, shader and resource handling |
| G3 character/animation/combat | Very high / high | Contact, timing, movement ownership, gameplay edge cases |
| G4 directing/motion matching | Very high / research-heavy | Semantic action resolution, temporal continuity, bake fidelity |
| G5 production reliability | Very high / open-ended until representative content exists | Scale, recovery, compatibility, long-tail bugs |
| Mobile/MMO | Separate major programs | Platform qualification and distributed systems are not minor backends |

These are planning judgments, not externally sourced productivity figures.

## 23.2 Work-item standard

Each item includes a narrow goal, owned files/modules, prerequisites, observable result, test procedure, failure cases, and evidence paths. Avoid 'implement animation system' as a single unchecked ticket.

Use one lead integrator. Parallel workers can implement independent fixtures, parsers, tests, or documented modules, but must not concurrently redesign the same scene schema, command protocol, RHI, or movement authority. Shared contracts change through small reviewed decision records.

A useful implementation cycle is: describe expected behavior, add fixture/test, implement the smallest working path, run actual checks, inspect output, record limitations, and commit. No test suite may be replaced by unconditional success to keep the cycle green.

## 23.3 Bounded research

Research branches have an explicit question and comparison. Examples: does Forward+ improve the many-light fixture; does motion matching improve starts/stops on the cleared dataset; does a new GI method improve the hall within the measured budget?

Failure to improve the chosen metric can result in reverting the experiment. That is a technical result, not an excuse to keep an unmaintained subsystem because it took effort.

Stop and report when a decision would require production edits, paid resources, unclear content rights, a second native graphics backend, a major schema rewrite, or unapproved public distribution. Ordinary implementation details inside the approved gate do not require repeated permission requests.

## 23.4 Required implementation report

Every report contains: goal and scope; commit; platform/toolchain/GPU; files/modules changed; actual commands executed and exit results; passed/failed/not-run tests; screenshots/recordings with provenance; defects and limitations; safety-boundary check; and next proposed task.

Use a checklist that maps directly to `docs/VALIDATION_MATRIX.md`. A final report cannot call a gate complete while required tests are merely planned.

# 24. Risk register

Ratings are qualitative planning assessments, not measured probabilities.

| Risk | Severity | Control / evidence required |
|---|---|---|
| Engine work displaces Origins | High | Separate repo and first assignment; no production migration; explicit review after G1 |
| AI writes plausible but incorrect GPU code | High | Validation layers, physical-device tests, frame capture, no mock passes |
| Available host lacks suitable graphics capability | High | G0 inventory; headless/GPU status separated; no unsupported platform claims |
| Current assets cannot legally or practically move | High | Original fixtures; per-item provenance/quarantine; source export review |
| Renderer never reaches desired visual quality | High | Early art review at G2, motion review, realistic content and fixed budgets |
| Animation/motor/contact integration is poor | High | One movement authority; root-motion and contact fixtures before advanced search |
| Editor/API corruption loses work | High | Revision checks, staged validation, atomic writes, journal/recovery tests |
| Large scene/asset memory behavior breaks | High | Explicit residency limits, cache invalidation, repeated load/unload traces |
| Untested determinism or replay claims | High | Qualified-build scope; event ordering; separate recorded playback from resimulation |
| Mobile or MMO requirements distort early architecture | High | Profiles and interfaces now; separate implementation/qualification programs later |
| Dependency updates break reproducibility | Medium-high | Immutable pins, checksums, one acquisition method, isolated update changes |
| Multiple agents create incompatible authorities | High | Lead integrator; shared contract ownership; no concurrent RHI/schema rewrites |
| Debug UI mistaken for production UI | Medium-high | Separate player-UI gate, localization/accessibility/controller tests |
| Packaging works only on the developer's machine | High | Clean output directory and alternate-machine testing; no source/editor paths |
| Long-tail maintenance exceeds benefit | High | Workflow comparison at G5/G6; retain useful parts without forced adoption |

The highest-risk unknowns are not how quickly code can be generated. They are whether real content can be imported lawfully and correctly, whether characters move and act convincingly, whether rendering holds up in motion, and whether changes remain reproducible.

# 25. Migration and continuation decisions

## 25.1 Continuing engine research

Continue past a gate when its intended capability is verified, limitations are understood, and the next question remains worth investigating. A rough-looking but correct editor can earn another stage. A beautiful static scene with a broken data model cannot.

An inability to test on the current host may be a qualification block rather than a design failure. Record it explicitly and preserve the verified work.

## 25.2 Adopting the engine for a game

The engine need not outperform Unreal in every metric. It must meet the particular game's non-negotiable quality, reliability, platform, and performance needs, while offering enough benefit to justify migration and long-term ownership.

Require a representative rather than toy comparison. For Origins, that means character close-ups, real movement/combat, a cinematic transition, relevant effects, authored encounter progression, save/checkpoint behavior, and an independently runnable package. A hall made of boxes cannot decide this question.

For a bounded arena game, the first comparison may be smaller, but real target-device performance and UI still matter. For an MMO, engine rendering success cannot substitute for backend/network qualification.

## 25.3 Migration accounting

Inventory each source asset, gameplay system, mission, cinematic, UI surface, plugin dependency, and packaging requirement. Classify it as reusable source, convertible with tested loss, rewrite, replace, or blocked. Include manual fixes and regression coverage in the migration cost.

Keep production Unreal projects usable throughout evaluation. Any bridge/export experiment uses copies and produces a manifest/diff. No automated importer may overwrite the source project.

## 25.4 Useful outcomes short of replacement

A standalone cinematic tool, portable combat simulator, asset-validation pipeline, agent command framework, or renderer research bed can be valuable even if a full game never migrates. Those outcomes do not require relabeling the project as an Unreal replacement before it qualifies.

# 26. Open decisions and source register

## 26.1 Decisions to resolve with evidence

| ID | Question | Default until evidence changes it |
|---|---|---|
| ADR-001 | Which host/toolchain/GPU can Caelis actually use? | Inventory in G0; never assume remembered hardware is current |
| ADR-002 | First real graphics backend? | Vulkan 1.3; no simultaneous D3D12/Metal implementation |
| ADR-003 | Exact dependency revisions and licenses? | Pin and preserve notices in G0; no floating versions |
| ADR-004 | First game profile? | Original sandbox only; no production game adoption |
| ADR-005 | Source scene storage granularity? | One strict JSON file through G1; revisit with collaboration data |
| ADR-006 | Initial command durability? | Journal/recovery design tied to single-scene commit; explicit tested guarantee |
| ADR-007 | Reference hardware and finalized budgets? | Record first available qualified machine; targets in Section 21 remain provisional |
| ADR-008 | Cleared source character/animation dataset? | Original primitives first; imported dataset quarantined until reviewed |
| ADR-009 | Engine publication/license? | Private research; do not choose a public license on Matthew's behalf |
| ADR-010 | Need for advanced GI or virtual geometry? | Separate later experiments driven by representative content |
| ADR-011 | Mobile renderer and packaging route? | Separate future profile; not implied by desktop success |
| ADR-012 | Native Cinemafy plan/compiler format? | Preserve authored intent, resolved plan, and executed take as separate data |

## 26.2 Project source

**[P1]** `Sovereign Call_ Origins Technical Design Document-20251209034722.md`, supplied project document. Used only for the legacy system inventory and broad integration requirements. Newer combat rules, current code status, and current level roster were not established from this file.

## 26.3 Primary public sources

These references support dependency capabilities and terms, not performance promises or proof that this proposed integration works. Retrieved during preparation on October 16, 2026. Exact versions used for implementation must be recorded separately.

**[S1] SDL official repository.** Platform/window/input foundation and license.  
https://github.com/libsdl-org/SDL

**[S2] Khronos Vulkan versions/porting guide and dynamic-rendering synchronization guide.** Vulkan 1.3 design context; explicit rendering and synchronization requirements.  
https://github.khronos.org/Vulkan-Site/guide/latest/versions.html  
https://github.khronos.org/Vulkan-Site/tutorial/latest/Synchronization/Dynamic_Rendering_Sync/01_introduction.html

**[S3] volk official repository.** Vulkan API loading library.  
https://github.com/zeux/volk

**[S4] Vulkan Memory Allocator official repository.** Vulkan allocation helper; MIT license.  
https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator

**[S5] Microsoft DirectX Shader Compiler official repository.** HLSL compilation including DXIL and SPIR-V.  
https://github.com/microsoft/DirectXShaderCompiler

**[S6] EnTT official repository.** ECS storage and current integration requirements. Code and documentation have distinct license notices; preserve the relevant ones.  
https://github.com/skypjack/entt

**[S7] Dear ImGui official repository.** Tool UI, supported backends, MIT license, and stated internationalization/accessibility limitations.  
https://github.com/ocornut/imgui

**[S8] Khronos glTF 2.0 specification.** Coordinate, transform, material, skin, and animation interchange semantics.  
https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/Specification.adoc

**[S9] cgltf official repository.** glTF parsing/validation foundation.  
https://github.com/zeux/cgltf

**[S10] Jolt Physics official repository.** Physics foundation and MIT license.  
https://github.com/jrouwe/JoltPhysics

**[S10b] Jolt Physics 5.0.0 documentation, Deterministic Simulation section.** Version-specific explanation of determinism prerequisites. This is not asserted to be the latest Jolt documentation; recheck the selected revision before relying on its guarantees.  
https://jrouwe.github.io/JoltPhysicsDocs/5.0.0/index.html

**[S11] Recast/Detour official repository.** Navigation building/query responsibilities and license.  
https://github.com/recastnavigation/recastnavigation

**[S12] miniaudio official repository.** Audio foundation and licensing information.  
https://github.com/mackron/miniaudio

**[S13] nlohmann/json official repository.** Proposed authoring JSON parser/serializer.  
https://github.com/nlohmann/json

**[S14] Unreal Engine EULA.** Engine code/content definitions, distribution, and related rights. This TDD's no-Epic-code policy is an independent implementation boundary, not a claim that every kind of interoperability is legally prohibited.  
https://www.unrealengine.com/eula/unreal

**[S15] Lua official license page.** Reference for a deferred scripting option only.  
https://www.lua.org/license.html

**[S16] Fab EULA page and Standard License summary.** The summary says compatible tools are not limited to Unreal; review the binding agreement and particular asset grant, not just the summary.  
https://www.fab.com/eula

**[S17] Epic Content License Agreement.** UE-Only designation and content-specific restrictions; review applicable acquisition terms.  
https://www.unrealengine.com/eula/content

---

## Closing implementation directive

**Build the first independent scene-authoring and runtime loop. Make the real path visible, make failures honest, and leave the existing games untouched. The next decision should be based on a working artifact, not another impressive description of one.**
