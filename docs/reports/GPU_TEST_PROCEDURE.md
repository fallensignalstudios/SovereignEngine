# GPU qualification procedure (G1-GPU items)

This host had no physical GPU (ADR-001), so every G1 item that needs **native rendering on a named
physical GPU** is BLOCKED. This is the procedure to run on a machine that has one. It changes nothing
outside the repository checkout and its `build/` directory.

## 0. Requirements

- Windows 10/11 x64 or Linux x86_64 with a discrete or integrated GPU whose driver supports **Vulkan
  1.3** (`dynamicRendering`, `synchronization2`, `timelineSemaphore`), plus the Vulkan loader.
- Khronos validation layer (`VK_LAYER_KHRONOS_validation`), e.g. from the LunarG Vulkan SDK (Windows)
  or `vulkan-validationlayers` (Ubuntu). `vulkan.rendering` fails without it; the editor and runtime
  print a warning and continue without validation, so check that warning is absent from their logs.
- CMake >= 3.25, Ninja, a C++20 compiler (MSVC 19.38+, GCC 13+ or Clang 17+), git, Python 3.
- Linux only: the X11 development headers in `dependencies.lock.json` (SDL3 `transitive`), and a
  one-time DXC build: `scripts/build_dxc_from_source.sh` (~16 min on the 4-vCPU recording host; ADR-015).
- Windows note: the Windows path has **never been compiled** (no Windows host was available). Expect
  to fix small portability errors; record each as a defect rather than working around it silently.
  The windowed CTest entries use bash scripts (`scripts/ctest_display_launcher.sh`); on Windows run
  them from Git Bash, or run the editor/runtime commands in section 3 by hand.

## 1. Record the machine

```
git rev-parse HEAD
vulkaninfo --summary > gpu_host_vulkaninfo.txt
```

Also note OS build, CPU, RAM, GPU model, VRAM, driver version, display resolution and refresh rate.

## 2. Build and run everything

```
cmake --preset dev-headless && cmake --build --preset dev-headless && ctest --preset dev-headless
cmake --preset dev-vulkan   && cmake --build --preset dev-vulkan
ctest --preset dev-vulkan   --output-on-failure      2>&1 | tee dev_vulkan_ctest.log
ctest --preset gpu-qualify  --output-on-failure      2>&1 | tee gpu_qualify_ctest.log
```

`gpu-qualify` runs every `vulkan`-labelled test with `SOV_REQUIRE_PHYSICAL_GPU=1`: the device
selector **refuses** CPU implementations (llvmpipe, SwiftShader, WARP-like layers), so a pass cannot
come from software rendering. On a machine with a real display, windowed tests use it; otherwise they
use a private Xvfb display (Linux) or are skipped (never passed).

## 3. Acceptance check (what "GPU-tested" means for G1)

All of the following, on the named physical GPU:

| ID | Check | Where to look |
|---|---|---|
| G1-GPU-01 | `gpu_qualify_ctest.log` ends `100% tests passed`, 0 skipped | log tail |
| G1-GPU-02 | each test log prints `physical device: <your GPU> (...) -> eligible` and the selected device is that GPU, not a CPU device | `build/dev-vulkan/Testing/Temporary/LastTest.log` |
| G1-GPU-03 | `vulkan.rendering` summary reports **0** validation errors and **0** validation warnings (synchronization validation on) | `zz_validation_summary_after_suite` output |
| G1-GPU-04 | Axis Lab orientation/depth/culling/pick-ID test and First Light render test pass | `vulkan.rendering` |
| G1-GPU-05 | runtime capture `build/dev-vulkan/test-artifacts/runtime_first_light.png` shows the hall; its tEXt `EvidenceClass` is `gpu-rendered (physical GPU)` and `Device` names your GPU | `python3 scripts/png_provenance.py <png>` |
| G1-GPU-06 | editor automation report: every step `ok`, 50 save/play/stop/reopen cycles return to the resource baseline | `build/dev-vulkan/test-artifacts/editor/editor_report.json` |
| G1-GPU-07 | window lifecycle stress: no resource growth (exit 0) | `vulkan.runtime_window_lifecycle_stress` |
| G1-GPU-08 | real-input check (Linux): CLI write refused (exit 3) while editor open; journal shows the edit | `build/dev-vulkan/test-artifacts/gui_real_input/` |

Then, interactively (human-reviewed, not automatable):

```
build/dev-vulkan/bin/sovereign-editor  --scene content/authoring/first_light.scene.json
build/dev-vulkan/bin/sovereign-runtime --scene content/authoring/first_light.scene.json
```

Click to select, drag inspector fields, Ctrl+Z / Ctrl+Y (or Ctrl+Shift+Z), Ctrl+S, F5 to play/stop,
right mouse + WASD/QE to fly the editor camera (Esc releases the mouse), F to focus, Delete, resize,
minimize, close. In the runtime: WASD/QE fly, Shift faster, right mouse button captures the mouse for look. Confirm presentation looks
right on a real monitor (sRGB, no tearing in FIFO mode, no flicker on resize).

## 4. Performance baseline (only after section 3 passes; ADR-007)

```
build/dev-vulkan/bin/sovereign-runtime --scene content/authoring/first_light.scene.json \
  --width 1920 --height 1080 --frames 3000 --no-vsync --no-validation
```

Release build recommended (`-DCMAKE_BUILD_TYPE=Release` on a copy of the preset). Record p50/p95/p99
frame time over several runs with warm caches, per TDD §21.3. No performance result exists yet; do
not compare against numbers from the software-rendered host.

## 5. Report back

Put `gpu_host_vulkaninfo.txt`, both ctest logs, the PNGs and `editor_report.json` under
`docs/reports/evidence/gpu/<machine-name>/`, and update the G1-GPU rows of the G0/G1 report from
BLOCKED to the actual result (PASS or FAIL with the failing output).
