# ADR-001: Host, toolchain and GPU actually available

Status: Accepted for the toolchain. **Pending** for the GPU (no physical GPU on this host).
Date: 2026-10-16. Evidence: `docs/reports/HOST_INVENTORY.md`, `docs/reports/evidence/host/`.

## Context

TDD §4.3 and ADR-001's default: inventory the host in G0 and never assume remembered hardware.

## Findings

The assignment ran in a managed Linux cloud container (no access to Matthew's workstation):

- Ubuntu 24.04.5 LTS, x86_64, kernel 6.18 (KVM guest), 4 vCPU Intel Xeon @ 2.10 GHz, 15 GiB RAM.
- GCC 13.3.0 (default), Clang 18.1.3 (available, not used for the recorded builds), CMake 3.28.3,
  Ninja 1.11.1, Python 3.13, git 2.43.
- **No physical GPU.** No PCI display adapter, no `/dev/dri`. The only Vulkan implementation is Mesa
  25.2.8 **llvmpipe (lavapipe)**, `VK_PHYSICAL_DEVICE_TYPE_CPU`, Vulkan 1.4.318 (conformance 1.3.1.1).
  Loader 1.3.275; `VK_LAYER_KHRONOS_validation` 1.3.275.
- No physical display. Windowed tests use a private Xvfb + openbox display (`scripts/with_virtual_display.sh`).
- Installed for this assignment from the Ubuntu archive (free, container-local, recorded in
  `docs/reports/evidence/host/apt_installed_this_assignment.log`): mesa-vulkan-drivers,
  vulkan-validationlayers, vulkan-tools, xvfb, xauth, xdotool, x11-utils, imagemagick, openbox and the
  X11 development headers SDL3 needs. Nothing else on the host was changed.

## Decision

- Recorded builds use GCC 13.3 + Ninja through the presets. Clang is not part of the evidence.
- Every graphics result from this host is classed **software-rendered (CPU Vulkan implementation)**
  (ADR-013). No result from this host is "GPU-tested" in the TDD §1.3 sense.
- GPU qualification is **blocked** here. `ctest --preset gpu-qualify` refuses CPU devices and is the
  procedure for a real GPU host (`docs/reports/GPU_TEST_PROCEDURE.md`).
- Windows and macOS were not available and are **untested**; no claim is made for them. The code has
  Windows paths (`LockFileEx`, `ReplaceFileW`, prebuilt DXC) that have never been compiled.
