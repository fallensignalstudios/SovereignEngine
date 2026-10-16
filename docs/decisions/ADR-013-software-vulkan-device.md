# ADR-013: lavapipe as supplementary, software-rendered evidence

Status: Accepted, with limits. Date: 2026-10-16.

## Context

The host has no physical GPU, only Mesa's CPU Vulkan implementation (llvmpipe/lavapipe). The handoff:
do not mark G1 complete, fake a screenshot or substitute a mocked renderer when graphics cannot be
qualified. TDD §1.3: "A software Vulkan implementation does not qualify physical-GPU performance."

## Decision

Run the **real** engine renderer, editor and runtime against lavapipe, with validation and
synchronization validation enabled, as supplementary evidence. This exercises the actual Vulkan code
paths (device creation, swapchain, barriers, descriptors, dynamic rendering, readback, resize) against
a conformant Vulkan implementation and the Khronos validation layer. It is not a mock.

Every artifact produced this way is labelled **software-rendered (CPU Vulkan implementation)**: the
renderer's `DeviceDescription.evidence_class`, the PNG tEXt provenance chunks, log lines and the
report. The default device selection ranks eligible devices by type (discrete, integrated, virtual, then
CPU), so a CPU device is chosen only when nothing better is eligible;
`require_physical_gpu` / `SOV_REQUIRE_PHYSICAL_GPU=1` (used by `ctest --preset gpu-qualify`) refuses them.

## What this evidence does and does not show

Shows: the code is valid Vulkan usage as far as the validation layers can tell; draws, depth, culling,
picking, outline, captures and lifecycle behave correctly on a conformant implementation.

Does not show: behaviour on any physical GPU driver (different tiling, memory types, timing, driver
bugs, present modes), performance, or visual review on a real display. G1-GPU items stay **BLOCKED**.
