# Host inventory (G0-01)

Collected 2026-10-16 on the machine that produced every result in this repository. Raw output:
`docs/reports/evidence/host/` (`host.txt`, `toolchain.txt`, `vulkaninfo_summary.txt`,
`apt_installed_this_assignment.log`). Nothing here is remembered or assumed.

## Machine

| Item | Value |
|---|---|
| Kind | Managed cloud container (Claude Code remote session), KVM guest, ephemeral |
| OS | Ubuntu 24.04.5 LTS (noble), x86_64 |
| Kernel | Linux 6.18.44 |
| CPU | Intel Xeon Processor @ 2.10 GHz, 4 vCPU (1 socket, 4 cores, 1 thread/core), AVX2/AVX-512F/FMA |
| RAM | 15 GiB, no swap |
| Disk | fixed per-session allowance (not the 252 GB the device reports) |
| GPU | **none**: no display-class PCI device (11 PCI devices, none class 0x03), no `/dev/dri` |
| Display | none (no `DISPLAY`/`WAYLAND_DISPLAY`); windowed tests use Xvfb 21.1.12 + openbox 3.6.1 |

## Vulkan

| Item | Value |
|---|---|
| Loader | libvulkan1 1.3.275 (instance version 1.3.275) |
| Only device | **llvmpipe (LLVM 20.1.2, 256 bits)**, `PHYSICAL_DEVICE_TYPE_CPU` |
| Driver | Mesa 25.2.8-0ubuntu0.24.04.4 (lavapipe), API 1.4.318, conformance 1.3.1.1 |
| Validation | `VK_LAYER_KHRONOS_validation` 1.3.275 (synchronization validation available) |

Evidence class for every graphics result: **software-rendered (CPU Vulkan implementation)** (ADR-013).

## Toolchain

| Tool | Version | Use |
|---|---|---|
| GCC / G++ | 13.3.0 (Ubuntu 13.3.0-6ubuntu2~24.04.1) | all recorded builds |
| libstdc++ | 14.2.0 runtime | |
| Clang | 18.1.3 | installed, not used for evidence |
| CMake | 3.28.3 | presets v6 |
| Ninja | 1.11.1 | |
| GNU ld | 2.42 | |
| Python | 3.13.16 | scripts, independent checks |
| git | 2.43.0 | FetchContent, revision checks |
| DXC | v1.8.2505.1 built from source @ b106a961d092 (`libdxcompiler.so: 1.9(dev;1-b106a961)`) | shaders (ADR-015) |
| xdotool / ImageMagick | 3.20160805.1 / 6.9.12-98 | real X11 input, X server captures |

## Changes made to the host

Ubuntu-archive packages installed for this assignment (free, container-local): mesa-vulkan-drivers,
vulkan-validationlayers, vulkan-tools, xvfb, xauth, xdotool, x11-utils, imagemagick, openbox, and the
X11 development headers SDL3 needs (libx11-dev, libxext-dev, libxrandr-dev, libxcursor-dev,
libxfixes-dev, libxi-dev, libxss-dev, libxtst-dev, libxkbcommon-dev). DXC was built into the
repository's ignored `build/tools/`. Nothing outside the container and this repository was changed.

## Not available (no inference)

- Physical GPU of any vendor: **blocked** GPU qualification (ADR-001, ADR-007).
- Windows and macOS hosts: **untested**. Windows code paths exist but have never been compiled.
- A physical display, HDR, multi-monitor, variable refresh: untested.
