# ADR-003: Dependency selection, pins and acquisition

Status: Accepted. Date: 2026-10-16. Source of truth: `dependencies.lock.json`.

## Decision

| Dependency | Pin | Why |
|---|---|---|
| nlohmann/json | v3.12.0 release archive, SHA-256 checked | strict authoring JSON with ordered keys; parser callback lets us reject duplicate keys |
| EnTT | v3.15.0 (commit verified) | ECS storage; **private** to `sov::World`, never in persisted data or public headers |
| SDL3 | release-3.2.30 (commit verified), static | window, events, input, Vulkan surface; SDL renderer/GPU/audio disabled |
| Vulkan-Headers | vulkan-sdk-1.4.341.0 | API headers; no loader linked (volk loads the system loader) |
| volk | vulkan-sdk-1.4.341.0 | function loading |
| VMA | v3.3.0 | memory allocation |
| Dear ImGui | v1.92.9b-docking | editor tool UI; editor only |
| DXC | v1.8.2505.1 | offline HLSL to SPIR-V (build tool); acquisition per ADR-015 |

Acquisition: CMake FetchContent only. Git dependencies are shallow clones of an upstream tag whose
checked-out commit must equal the lock's `revision`, else configure fails. Archives/tool packages are
URL downloads verified by SHA-256. `FETCHCONTENT_UPDATES_DISCONNECTED=ON`, so a populated
`build/<preset>/_deps` rebuilds without network (verified in the G0 evidence). The lock's SHA-256 is
compiled into every binary and printed by `--version`.

Notices: `third_party_notices/<name>/` holds the upstream licence files; their SHA-256 values are in
the lock and `static.license_notices_match_lock` compares both against the fetched sources.

## Not chosen

Package managers (vcpkg/Conan) were not used: one acquisition method keeps G0 reproducible and the
lock readable. Updating any pin is its own change with build/test evidence and a lock refresh.
