# Sovereign Engine / First Ignition

A technical design and first-assignment packet for Matthew and Caelis.

**This packet is not an engine implementation.** It contains the master TDD, implementation instructions, acceptance checks, and small machine-readable contract fixtures. The Python checker verifies the packet's example data, not native engine behavior, GPU output, or production readiness.

## Start here

Give Caelis `CAELIS_FIRST_IGNITION.md` together with the rest of this folder. His first implementation assignment is **G0 and G1 only**: a separate engine repository with a real native edit-save-run loop. The full roadmap preserves the ambition of an independent Unreal replacement without authorizing production migration now.

## Files

| File | Purpose |
|---|---|
| `Sovereign_Engine_TDD_v0.1.md` | Master architecture, system boundaries, gates, risks, evidence requirements, and sources |
| `Sovereign_Engine_TDD_v0.1.html` | Readable browser edition with section navigation; generated from the Markdown master |
| `CAELIS_FIRST_IGNITION.md` | Concrete first implementation assignment and required report |
| `AGENTS.md` | Repository instructions and non-negotiable boundaries for coding agents |
| `docs/VALIDATION_MATRIX.md` | G0/G1 acceptance IDs plus later-gate priority tests |
| `schemas/scene.schema.json` | Proposed strict G1 authoring-scene wire contract |
| `schemas/command_batch.schema.json` | Proposed typed command-batch wire contract |
| `examples/first_light.scene.json` | Original primitive scene with stable IDs; illustrative data, not a rendered scene |
| `examples/place_banner.command_batch.json` | A dry-run command fixture for that scene; not executed against an engine |
| `tools/validate_contract_examples.py` | Offline schema and limited semantic-fixture checks, including negative controls |
| `docs/PACKET_VALIDATION.txt` | Results from checking the delivered packet; not G0/G1 qualification |

## Important distinctions

The engine and its command-line executables do not yet exist in this delivery. Commands in the handoff specify interfaces Caelis should implement. The JSON contracts are proposed v0.1 formats, not compatible exports from Unreal or an existing engine.

The source TDD available during preparation was the December 2025 legacy game document. It informed system coverage, not current protagonist tuning, current code status, or a current level roster. The newer v2 outline mentioned previously was not retrieved. The engine's test-only combat numbers must not override the actual game design.

The selected initial stack is C++20, SDL3, native Vulkan, and small independently licensed foundations. Windows is the first game-delivery qualification target; exact host/GPU/toolchain capability must be discovered rather than assumed. Native macOS, mobile, consoles, and MMO operation are not implied by a portable headless build.

## Contract conventions

G1 scene entities use `id`, `name`, and typed `components`. The name is stored in the entity envelope and can be represented by a Name component internally. Every entity has a Transform; visual primitives also have PrimitiveMesh and MaterialParameters. A scene's `active_camera_id` must identify an entity with a Camera component.

A unit box has side length 1 and is centered on its local origin. A unit plane lies in XZ, has side length 1, and faces +Y. A unit cylinder is centered on its local origin, aligned with +Y, height 1, and radius 0.5. Transform scale gives dimensions in meters. Positive scale is the G1 source-scene restriction.

G1 materials carry linear base-color RGBA and emissive RGB for simple rendering. They do not claim physically based shading or global illumination. PBR asset/material extensions are a G2 schema/version decision.

The sample camera looks along local -Z, uses a right-handed Y-up basis, and has a normalized `[x,y,z,w]` quaternion. The engine must translate its projection/viewport convention correctly for Vulkan.

The command batch is a **dry run**. Changing it to a committed request belongs to Caelis's future test run; this packet has not applied it anywhere. Actor labels identify requests but are not authentication credentials.

## Checking this packet

With Python 3.10+ and the `jsonschema` package (including its `referencing` dependency) installed, run from this folder:

```sh
python tools/validate_contract_examples.py
```

The checker registers schemas locally and does not fetch schema URLs. It checks the included scene and command shapes, UUID/reference relationships, hierarchy, camera, quaternion/scale constraints, and several invalid fixtures. It does not implement transaction execution, file locking, durable idempotency, undo, or graphics.

## Implementation boundary

No changes to Origins/ProjectVelkorran or any other production game, Unreal installation, asset library, or plugin repository. No automatic asset migration, public publishing, paid service use, or external telemetry. Build the independent sandbox, qualify the result honestly, and report before expanding scope.
