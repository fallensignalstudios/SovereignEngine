# ADR-017: Adopt the owner's First Ignition packet as the contract

Status: Accepted. Date: 2026-10-16. Supersedes ADR-014.

## Context

The first round of this assignment had only the TDD v0.1, so ADR-014 reconstructed `AGENTS.md`, the
validation matrix, the schemas, First Light and the example batch, and said the originals would win
if they appeared. The owner then supplied `Sovereign_Engine_First_Ignition_Packet.zip`. Its contracts
differ from the reconstruction in substance: different scene and component field names, full
replacement `transform.set`/`material.set`, `dry_run` required, 128 operations per batch, no light or
environment data in the scene, a different First Light hall, and a different matrix (G0-01..08,
G1-D01..D16, G1-R01..R15).

## Decision

1. The packet is imported verbatim under `docs/packet/` (with its `MANIFEST.sha256.json`), and the
   files the repository uses are byte-identical copies at their packet paths: `AGENTS.md`,
   `docs/VALIDATION_MATRIX.md`, `schemas/scene.schema.json`, `schemas/command_batch.schema.json`,
   `examples/first_light.scene.json`, `examples/place_banner.command_batch.json`,
   `tools/validate_contract_examples.py`. `static.packet_files_verbatim` fails on any drift; changing
   them needs an owner-approved decision.
2. The engine implements contract v1 exactly: the reflection descriptors mirror the scene schema
   (`scene.schema_files_match_reflection_descriptors`), operations use the packet wire shapes, and
   every scene, batch and fixture in the repository is checked against the owner's schemas by an
   independent validator (`static.repository_files_conform_to_owner_schemas`, python `jsonschema`),
   besides the packet's own checker (`static.packet_contract_checker`).
3. `content/authoring/first_light.scene.json` is the packet's First Light, imported through
   `sovereign-cli scene import` (validated, canonical, journalled, revision 0), so the packet example
   batch (`base_revision` 0) applies to it unchanged. The reconstructed hall and its build batch are
   retired; Axis Lab (the orientation fixture) is regenerated in packet shapes.
4. **R0 fixed lighting.** Contract v1 carries no lights, so R0 lights every scene the same way: one
   warm key light (direction `normalize(0.35, -1, -0.55)`, radiance `(3.2, 3.0, 2.8)`), hemispheric
   ambient `(0.10, 0.10, 0.12)`, dielectric roughness 0.6, exposure 1.0. `background_linear_rgb` is the
   clear colour. Authored lights are a contract change for a later gate.
5. **Alpha.** `base_color_rgba[3]` is stored, round-tripped and diffed, but R0 renders opaque; a
   scene with alpha < 1 loads with a warning saying so.
6. `schemas/command_result.schema.json` stays engine-defined (the packet has no result contract).
   Failures that happen before a batch is evaluated (writer lock unavailable, I/O, unreadable request)
   print the CLI error envelope instead (docs/architecture/command_protocol.md).

## Consequences

- The reconstructed `AGENTS.md` moved to `docs/DEVELOPING.md` as the repository guide; the packet's
  `AGENTS.md` is the rule set.
- The report maps evidence to the packet's matrix IDs only.
- Earlier evidence (commit 5a7b2ee and before) used the reconstructed contract and is superseded by
  the regenerated set.
