# Decision records

TDD §26.1 lists the open decisions ADR-001 to ADR-012 with defaults. This directory records the ones
G0/G1 had to settle, plus decisions this assignment raised (ADR-013 onward). Status values:
**Accepted** (in force), **Provisional** (in force until the named evidence arrives), **Pending**
(not decidable on this host or not yet in scope).

| ADR | Title | Status |
|---|---|---|
| [001](ADR-001-host-and-toolchain.md) | Host, toolchain and GPU actually available | Accepted (inventory); GPU part Pending |
| [002](ADR-002-vulkan-first-backend.md) | Vulkan 1.3 as the only graphics backend | Accepted |
| [003](ADR-003-dependency-pins.md) | Dependency selection, pins and acquisition | Accepted |
| [004](ADR-004-sandbox-game-profile.md) | First game profile: original sandbox only | Accepted |
| [005](ADR-005-scene-storage.md) | One strict canonical JSON file per scene | Accepted through G1 |
| [006](ADR-006-command-durability.md) | Journal-backed single-scene commits | Accepted, guarantee stated |
| [007](ADR-007-reference-hardware.md) | Reference hardware and budgets | **Pending**: no physical GPU on this host |
| 008 | Cleared character/animation dataset | Not opened: G1 uses original primitives only |
| 009 | Engine publication/licence | Not decided, by design: private research, no licence chosen for Matthew |
| 010-012 | Advanced GI, mobile route, native Cinemafy format | Not opened in G0/G1 |
| [013](ADR-013-software-vulkan-device.md) | lavapipe as supplementary software-rendered evidence | Accepted, with limits |
| [014](ADR-014-reconstructed-packet-files.md) | Reconstructed packet files (AGENTS.md, matrix, schemas, batch) | Provisional until originals are supplied |
| [015](ADR-015-dxc-source-build.md) | Build DXC from source on non-Windows hosts | Accepted, **needs Matthew's review** |
| [016](ADR-016-backend-boundary.md) | Backend boundary at the renderer API; no Null RHI | Accepted for G1 |
