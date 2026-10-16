# ADR-016: Backend boundary at the renderer API; no Null RHI in G1

Status: Accepted for G1. Date: 2026-10-16.

## Decision

- The renderer's public header (`engine/renderer/include/sov/renderer/renderer.h`) exposes no Vulkan
  types: views, cameras, draw options, picks, captures and provenance only. Callers (editor, runtime,
  tests) never see `Vk*` handles. The single exception is the opt-in `vulkan_native.h` used by the
  editor to host the Dear ImGui Vulkan backend.
- Inside the renderer, `engine/rhi/vulkan` is a thin set of Vulkan helpers (device, swapchain,
  resources, barriers, deferred destruction), not an abstract RHI.
- **Deviation from TDD §10.3.** The TDD's RHI contract (typed descriptors and generation-checked handles
  for buffers, textures, samplers, pipelines, command lists and submission completion) is **not**
  implemented as a separate API in G1. What is kept: backend objects are private to the renderer,
  public handles (`ViewHandle`) are index + generation checked, there is one graphics queue and two
  frame contexts, and resources retire on timeline completion (§10.3-10.4). Reason: with exactly one
  backend and one consumer (the R0 renderer), a general RHI layer would be designed without a second
  user to test it against. It should be introduced when R1's render graph needs it.
- No `engine/rhi/null`. Headless builds do not link the renderer at all, so there is no code path that
  can "render" without a device and report success. TDD §1.3: a Null backend does not prove rendering.

## Consequences

Adding a second backend later (only with Matthew's approval) means introducing the RHI interface then,
behind the same renderer API. The editor's ImGui integration is the one place that would need a
second implementation.
