# GPU qualification procedure (G1-R rows on a physical GPU)

The build host had no physical GPU (ADR-001): its only Vulkan device is llvmpipe, a CPU
implementation. G1-R01 (real native rendering on a named physical GPU) and G1-R13 (baseline
performance on a recorded machine) are therefore **BLOCKED**, and every other G1-R result from that
host carries the evidence class *software-rendered (CPU Vulkan implementation)*. This is the
procedure to run on a machine with a physical GPU. It changes nothing outside the repository
checkout and its `build/` directory, spends nothing and publishes nothing.

## 0. Requirements

- Windows 10/11 x64 or Linux x86_64 with a discrete or integrated GPU whose driver supports **Vulkan
  1.3** (`dynamicRendering`, `synchronization2`, `timelineSemaphore`), plus the Vulkan loader.
- Khronos validation layer (`VK_LAYER_KHRONOS_validation`), e.g. from the LunarG Vulkan SDK (Windows)
  or `vulkan-validationlayers` (Ubuntu). Every Vulkan CTest runs with `SOV_REQUIRE_VALIDATION=1` and
  fails with a diagnostic if the layer is missing.
- CMake >= 3.25, Ninja, a C++20 compiler (MSVC 19.38+, GCC 13+ or Clang 17+), git, Python 3 with
  `jsonschema` (for the contract checks; without it they are reported skipped, not passed).
- Linux only: the X11 development headers in `dependencies.lock.json` (SDL3 `transitive`), Xvfb +
  openbox + xdotool + ImageMagick for the scripted real-input test when no desktop is running, and a
  one-time DXC build: `scripts/build_dxc_from_source.sh` (~16 min on the 4-vCPU recording host; ADR-015).
- Windows note: the Windows path has **never been compiled** (no Windows host was available). Expect
  to fix small portability errors and record each as a defect. The windowed CTest entries use bash
  scripts; run them from Git Bash, or run the commands in section 3 by hand.

## 1. Record the machine

```
git rev-parse HEAD
vulkaninfo --summary > gpu_host_vulkaninfo.txt
```

Also note OS build, CPU, RAM, GPU model, VRAM, driver version, display resolution and refresh rate.
If the machine has more than one GPU, choose one explicitly: `--device "<name substring>"` on the
editor and runtime, or `SOV_VULKAN_DEVICE="<name substring>"` for the tests.

## 2. Build and run everything

```
cmake --preset dev-headless && cmake --build --preset dev-headless && ctest --preset dev-headless
cmake --preset dev-vulkan   && cmake --build --preset dev-vulkan
ctest --preset dev-vulkan   --output-on-failure      2>&1 | tee dev_vulkan_ctest.log
ctest --preset gpu-qualify  --output-on-failure      2>&1 | tee gpu_qualify_ctest.log
```

`gpu-qualify` runs every `vulkan`-labelled test with `SOV_REQUIRE_PHYSICAL_GPU=1`: the device
selector **refuses** CPU implementations (llvmpipe, SwiftShader and similar), so a pass cannot come
from software rendering (G1-R15). On the recording host it fails by design.

## 3. Acceptance check, by matrix ID

All of the following, on the named physical GPU:

| ID | Check | Where to look |
|---|---|---|
| G1-R01 | `gpu_qualify_ctest.log` ends `100% tests passed`, 0 skipped; each test log prints `physical device: <your GPU> (...) -> eligible` and `evidence class: gpu-rendered (physical GPU)` | log tail; `build/dev-vulkan/Testing/Temporary/LastTest.log` |
| G1-R02 | `rendering.axis_lab_orientation_depth_culling_hierarchy_and_pick_ids` and `rendering.projection_matches_analytic_fov_and_aspect` pass | `vulkan.rendering` |
| G1-R03 | `rendering.request_pick_resolves_entity_with_frame_table`, `rendering.pick_resolves_with_the_drawn_snapshot_not_a_stale_table` pass; real-input pick step passes | `vulkan.rendering`, `test-artifacts/gui_real_input/gui_real_input.log` |
| G1-R04 | `vulkan.runtime_window_lifecycle_stress` exits 0 (no resource growth); `rendering.view_resize_and_recreate_cycles_return_to_resource_baseline` passes | ctest log |
| G1-R05 | `rendering.zz_validation_summary_after_suite` reports **0** errors and **0** warnings with synchronization validation on; no editor/runtime log contains `vulkan-validation` | ctest log, `LastTest.log` |
| G1-R06, R07, R11 | editor automation report: every step `ok`; the `cycles` step shows 50 save/play/stop/reopen cycles with resource counts and VMA bytes equal to baseline and settled RSS | `test-artifacts/editor/editor_report.json` |
| G1-R08 | `vulkan.runtime_first_light` passes; `static.runtime_has_no_editor_code` passes | ctest log |
| G1-R09 | `vulkan.editor_real_x11_input` passes (Linux) and a person repeats the steps below | `test-artifacts/gui_real_input/` |
| G1-R12 | captures carry `EvidenceClass = gpu-rendered (physical GPU)` and `Device` = your GPU | `python3 scripts/png_provenance.py build/dev-vulkan/test-artifacts/*.png build/dev-vulkan/test-artifacts/*/*.png` |
| G1-R14 | `vulkan.failure_paths_unwind_cleanly` passes (15 cases, no hang, no validation output) | `test-artifacts/failure_paths/failure_paths.json` |
| G1-R15 | with `SOV_REQUIRE_PHYSICAL_GPU=1` the GPU is accepted (the failure-path case `runtime_physical_gpu_required_on_gpu_host` passes) | same |

Then, interactively (human review, G1-R09; not automatable):

```
build/dev-vulkan/bin/sovereign-editor  --scene content/authoring/first_light.scene.json
build/dev-vulkan/bin/sovereign-runtime --scene content/authoring/first_light.scene.json
```

Click to select in the viewport and the hierarchy, type and drag inspector fields (position,
base colour, emissive), Ctrl+Z / Ctrl+Y (or Ctrl+Shift+Z), Ctrl+S, F5 to play/stop, right mouse +
WASD/QE to fly the editor camera (Esc releases the mouse), F to focus, Delete, open the Diagnostics
tab, resize, minimize, close. In the runtime: WASD/QE fly, Shift faster, right mouse captures the
mouse for look. Confirm presentation on a real monitor: sRGB looks right, no tearing in FIFO mode,
no flicker on resize.

## 4. Performance baseline (G1-R13; only after section 3 passes; ADR-007)

Use a Release build (`-DCMAKE_BUILD_TYPE=Release` on a copy of the dev-vulkan preset). Close other
GPU work. For each setting, run three times with warm caches:

```
R=build/release-vulkan/bin/sovereign-runtime
$R --scene content/authoring/first_light.scene.json --width 1920 --height 1080 --frames 3120 --warmup 120 \
   --no-vsync --no-validation --perf-report perf_1080p_novsync_run1.json
$R --scene content/authoring/first_light.scene.json --width 1920 --height 1080 --frames 3120 --warmup 120 \
   --perf-report perf_1080p_vsync_run1.json
```

Each report records the commit, build type, device, driver, resolution, vsync, validation, warm-up,
scene hash, instance and triangle counts, and p50/p95/p99/max of frame time, renderer CPU work,
renderer blocked time and GPU time (timestamp queries), with process RSS and the VMA peak. TDD §21.3
budgets are provisional; report misses as misses. Never compare these numbers with the
software-rendered reports from the recording host.

## 5. Report back

Put `gpu_host_vulkaninfo.txt`, both ctest logs, the PNGs, `editor_report.json`, the
`gui_real_input/` and `failure_paths/` directories and the perf reports under
`docs/reports/evidence/gpu/<machine-name>/`, and update G1-R01..R15 in the G0/G1 report from
software-rendered or BLOCKED to the actual result (PASSED, or FAILED with the failing output).
