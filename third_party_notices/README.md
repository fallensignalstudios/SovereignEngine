# Third-party notices

Upstream licence files for every dependency pinned in `dependencies.lock.json`, copied verbatim from
the fetched sources. Their SHA-256 values are recorded in the lock; `ctest -R static.license_notices_match_lock`
fails if a copy here, the lock, or the fetched upstream file disagree.

| Dependency | Licence | Files here | Linked into |
|---|---|---|---|
| nlohmann/json v3.12.0 | MIT | `nlohmann_json/LICENSE.MIT` | editor, runtime, CLI, tests |
| EnTT v3.15.0 | MIT (code). Docs CC BY 4.0 and logos CC BY-SA 4.0 are **not** redistributed | `EnTT/LICENSE` | editor, runtime, CLI, tests |
| SDL3 release-3.2.30 | Zlib | `SDL3/LICENSE.txt` | editor, runtime |
| Vulkan-Headers vulkan-sdk-1.4.341.0 | Apache-2.0 OR MIT | `Vulkan-Headers/LICENSE.md`, `Apache-2.0.txt`, `MIT.txt` | headers only |
| volk vulkan-sdk-1.4.341.0 | MIT | `volk/LICENSE.md` | editor, runtime |
| Vulkan Memory Allocator v3.3.0 | MIT | `VulkanMemoryAllocator/LICENSE.txt` | editor, runtime |
| Dear ImGui v1.92.9b-docking | MIT | `imgui/LICENSE.txt` | **editor only** |
| DXC v1.8.2505.1 | NCSA (source build, ADR-015) | `dxc/LICENSE.TXT`, `dxc/ThirdPartyNotices.txt`; for the opt-in prebuilt package: `dxc/LICENSE-LLVM.txt`, `dxc/LICENSE-MS.txt` | **build tool only, never shipped** |

Nothing has been distributed. This directory exists so that any future binary distribution (not
authorized in G0/G1) starts from a correct notice set. No licence has been chosen for Sovereign Engine
itself (ADR-009: Matthew's decision).
