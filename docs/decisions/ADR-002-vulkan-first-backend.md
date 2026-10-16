# ADR-002: Vulkan 1.3 as the only graphics backend

Status: Accepted. Date: 2026-10-16.

## Decision

One native backend: Vulkan 1.3 core, loaded through volk, memory through VMA. Required features:
`dynamicRendering`, `synchronization2`, `timelineSemaphore` (all core in 1.3). No D3D12, Metal,
OpenGL, SDL_GPU or second rendering framework (TDD §4.1, §10.1; handoff constraints).

Shape of the G1 backend (`engine/rhi/vulkan`):

- one graphics+present queue; submissions signal a single timeline semaphore;
- two frame contexts (command pool/buffer, upload arenas); per-swapchain-image present semaphores;
- explicit lifetimes: objects released while the GPU may use them go through `Device::Retire`,
  destroyed once the timeline passes the submission that last used them;
- `synchronization2` barriers with tracked image layout/stage/access;
- validation layer + synchronization validation enabled by default in development and tests; any
  validation-type error fails tests and exits the executables with code 6.

## Consequences

Hosts without Vulkan 1.3 cannot run the editor or runtime. That is accepted: the alternative
(a fallback backend) is explicitly excluded. Headless targets do not need Vulkan at all.
