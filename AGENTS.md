# Agent instructions: Sovereign Engine research

## Mission and active scope

Build an independently implemented engine with its own editor, renderer, runtime, and authoring pipeline. The current assignment is G0/G1, as defined by `Sovereign_Engine_TDD_v0.1.md` and `CAELIS_FIRST_IGNITION.md`.

The full roadmap is not authorization to implement every future subsystem. Stop after the first gate report. Prioritize a real, reproducible edit-save-run loop over speculative abstractions or visual polish.

## Repository boundaries

Work only inside the isolated engine repository and its approved build/output directories. Never edit a production game, Unreal installation, unrelated repository, source asset library, or existing save. Resolve paths before filesystem writes. Preserve existing changes and commit history.

No paid services, public publishing, external telemetry, or asset upload without explicit authorization. Do not commit credentials, personal paths, downloaded source content, or entitlement evidence into a public branch.

## Independent code and dependencies

No copied Epic engine implementation. No hidden Unreal/Godot/Unity dependency or embedded complete renderer presented as our own.

Pin independent upstream dependencies by immutable revision/checksum and preserve their notices. Keep a dependency lock with build settings. Do not add a second package manager or floating `latest` fetch.

Use original primitives and shaders for G1. Third-party/production assets are quarantined until their rights and source format are reviewed. Owning a plugin project does not automatically make its Unreal dependencies portable.

## Architecture rules

- Core/world/validation tests must run without a graphics device.
- Editor, CLI, and agent edits use the same typed authoring services.
- Renderer receives immutable snapshots, not writable gameplay state.
- Persistent UUIDs are not ECS slots, physics IDs, or GPU handles.
- No Vulkan types in gameplay or persisted contracts.
- One owner writes world state; workers return results for controlled application.
- One character movement authority; cameras and animation cannot independently move actors.
- Scene documents, derived cooked data, save games, and recorded takes are distinct formats.
- API/schema changes require versioning, fixtures, and a decision record.
- Do not add empty modules or success-returning stubs to suggest progress.

## Command safety

Validate complete batches before mutation. Reject stale revisions and unknown fields. Keep revisions monotonic, including undo/redo. Bind idempotency keys to request content. A dry run must not write, increment revision, or masquerade as an applied edit.

Offline CLI writes acquire an exclusive scene lock. No unrestricted shell/eval endpoint. Localhost does not constitute authentication. Long cook/render/package jobs are not atomic scene transactions.

Use platform-appropriate atomic single-file replacement and a tested durability/recovery design. Do not call several independent writes atomic.

## Implementation defaults

C++20, CMake/Ninja/CTest, SDL3 platform/input, EnTT behind our world API, strict JSON authoring, Dear ImGui tool UI, HLSL/DXC, one native Vulkan backend with volk/VMA. Implement only dependencies needed by the current gate.

No C++ modules, general hot reload, custom lock-free scheduler, physics, motion matching, multiplayer, or mobile rendering in G1. Future requirements should influence boundaries, not force premature feature implementation.

## Verification and truthful reporting

Separate specified, implemented, compiled, headless-tested, GPU-tested, human-reviewed, and qualified states. Failed, blocked, and not-run checks remain visible.

Never claim a Null backend, software Vulkan renderer, mock, generated artwork, or static background as physical-GPU engine evidence. Never skip validation layers or weaken a test merely to obtain a pass.

Record exact commands, exit results, host/toolchain/GPU, dependency and content hashes, logs, and evidence paths. If a test cannot run, say why. Only mark a gate complete after all required checks pass for the claimed platform.

## Working method

Use small compilable changes and one integrating owner. Parallel work must respect declared file/module ownership. Shared contracts are not edited independently by multiple agents.

For each task: define the observable result, add a fixture/test, implement, execute the checks, inspect the output, document limitations, commit. Do not substitute another design essay for an implementation task.

Do not delete failing tests, erase work, force-push, or publish to solve a local development problem. Stop/report when blocked by an actual authority, licensing, platform, or scope boundary. Ordinary implementation choices inside scope do not require repeated questions.
