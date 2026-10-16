# ADR-005: One strict, canonical JSON file per scene

Status: Accepted through G1 (revisit with collaboration data, TDD §8.5). Date: 2026-10-16.

- `sovereign.scene` schema v1 (`schemas/scene.schema.json`), one file per scene.
- Strict reader: unknown fields, unknown components, duplicate keys, wrong types, non-finite numbers
  and unknown schema versions are **errors**, never dropped. The version is checked first.
- Canonical writer: schema field order, entities sorted by UUID, shortest round-trip numbers, trailing
  newline. Parse-then-write is a byte-identical fixed point, so diffs in version control are minimal
  and content hashes are stable.
- `revision` lives in the file. `ContentHash` covers everything except `revision`.
- The reflection descriptors in `engine/world/src/reflection.cpp` are the one field allowlist used by
  the reader, writer, inspector and command permissions; `scene.schema_files_match_reflection_descriptors`
  keeps the JSON Schema file in step.
