# G0 / G1 report: First Ignition

Date: 2026-10-16. Branch `research/first-ignition`. Author: Claude (Caelis role), for Matthew.
Gate checklist: `docs/VALIDATION_MATRIX.md`, the owner's packet copy (ADR-017).

**Bottom line.** G0 is complete on the recorded host (Linux x86_64, GCC 13.3). G1 is **implemented
against the owner's First Ignition contracts and tested, but NOT complete**: the host has no physical
GPU, so G1-R01 (named physical GPU), G1-R02 and G1-R03 (which the handoff requires "on a physical
GPU") and G1-R13 (baseline performance on a recorded machine) are **blocked**, and no person has
reviewed the editor yet. Every graphics result below was produced by the real engine on Mesa's CPU
Vulkan implementation (llvmpipe) and carries the evidence class **software-rendered**. There is no
Null backend, no mocked renderer and no generated or fabricated image anywhere in this repository.

Evidence classes (TDD §1.3, handoff): **H** native test without display or GPU, **S** static check of
sources or built binaries, **V** real Vulkan execution on the CPU implementation (software-rendered),
**X** real X server input and X server captures (Xvfb), **G** physical-GPU test (none exist yet),
**R** human review (none yet), **D** document or record. Each matrix ID is marked **passed**,
**failed**, **blocked** or **not run**; a "passed" G1-R row passed with evidence class V/X only and
must be repeated on a physical GPU (`docs/reports/GPU_TEST_PROCEDURE.md`) before G1 can qualify.

---

## 1. Identity: commit, host, toolchain, GPU, driver, lock, backend

| Item | Value |
|---|---|
| Code commit of every evidence binary | `fcbc52cc6b0f55680a40c99ca5f41266d5ce2630`, tree clean at build time (`--version` prints `(clean)`; every capture's `EngineCommit` says the same) |
| Report commit | the child of `fcbc52c` that adds this file and the regenerated evidence (plus the host-inventory ID fix and the narrowed isolation comparison in `scripts/collect_evidence.sh`; no engine code) |
| Dependency lock | `dependencies.lock.json` sha256 `c2ad80e256bb24a21da9321f9c562a1a2cb1d2393845987f493e1e80cb5a1065` (compiled into every binary) |
| Contracts | the owner's packet, verbatim: `docs/packet/` matches its `MANIFEST.sha256.json`; `AGENTS.md`, the matrix, both schemas, both examples and the checker are byte-identical copies (`static.packet_files_verbatim`) |
| Host | managed cloud container, KVM guest; Ubuntu 24.04.5 LTS x86_64, kernel 6.18.44; 4 vCPU Intel Xeon @ 2.10 GHz; 15 GiB RAM (`docs/reports/HOST_INVENTORY.md`) |
| Toolchain | GCC 13.3.0, CMake 3.28.3, Ninja 1.11.1, GNU ld 2.42, Python 3.13.16 + jsonschema 4.26.0, git 2.43.0; C++20, `-Werror` on engine code |
| GPU | **none** (no display-class PCI device, no `/dev/dri`) |
| Vulkan device used | `llvmpipe (LLVM 20.1.2, 256 bits)`, `PHYSICAL_DEVICE_TYPE_CPU`, Mesa 25.2.8-0ubuntu0.24.04.4, API 1.4.318, conformance 1.3.1.1; loader 1.3.275; `VK_LAYER_KHRONOS_validation` 1.3.275 with synchronization validation, required by every Vulkan test (`SOV_REQUIRE_VALIDATION=1`) |
| Active backend | native Vulkan 1.3 core (dynamic rendering, synchronization2, timeline semaphores) via volk + VMA; one graphics queue; 2 frame contexts; GPU timestamp queries |
| Shader compiler | DXC v1.8.2505.1 **built from source** @ `b106a961d092` (NCSA), `-spirv -fspv-target-env=vulkan1.3 -Zpc -HV 2021 -WX -O3` (ADR-015) |
| Display | Xvfb 21.1.12 + openbox 3.6.1 (private virtual display per test) |
| Evidence class of every image | **software-rendered (CPU Vulkan implementation)**, written into each PNG's tEXt chunks (ADR-013) |
| Tested platform | Linux x86_64 / GCC 13.3 only. **Not run:** Windows, macOS, Clang/MSVC builds, any physical GPU, any physical display |

## 2. Implemented modules and what each actually does

Everything in this repository is new in this assignment; nothing outside it was modified. 183 tracked
files outside the evidence and packet directories: about 11,800 lines of engine-owned C++20/HLSL,
2,900 lines of tests (95 headless tests in 9 suites, 10 Vulkan rendering tests) and 1,300 lines of
scripts. CTest: 18 entries in `dev-headless`, 29 in `dev-vulkan`. Dependency direction and the
enforced boundaries: `docs/architecture/modules.md`.

| Module | Path | What it does |
|---|---|---|
| core | `engine/core` | `Result`/`Error` codes, canonical UUIDs, SHA-256, logging, strict JSON (duplicate keys, depth, size, non-finite rejected) + canonical writer, durable file I/O (atomic replace, durable append, `flock`/`LockFileEx` writer lock that records and reports a reclaimed holder), math (Vec3/Quat/Mat4, reversed-Z projection), original PNG encoder with tEXt provenance, process memory query, build identity |
| world | `engine/world` | `SceneDocument` (contract v1) + reflection descriptors that mirror the owner's scene schema (the single field allowlist), strict version-first reader and canonical writer, structural validation (identity, hierarchy, active camera, quaternion norm, camera range), field-level diff, content hash, provenance label; EnTT-backed `World` behind a handle API, parent-before-child propagation, render snapshot, primitive geometry |
| commands | `engine/commands` | the five packet operations in wire shape, strict batch parser (limits from the contract), staged whole-batch validation with inverse operations, `SceneSession` (revisions, idempotency ledger bound to a content hash, undo/redo as new revisions), `SceneStore` (writer lock, JSONL journal, prepare/backup/replace/verify/commit, crash recovery) |
| runtime | `engine/runtime` | 60 Hz fixed-step `Simulation`, `GameModule` seam; `app/` = **`sovereign-runtime`** (opens a saved scene; `--device`, `--perf-report`, `--stress-window`, `--screenshot`) |
| platform | `engine/platform` | SDL3 video init, window, events, input action map |
| rhi/vulkan | `engine/rhi/vulkan` | instance/device selection (by name, refusing CPU devices on request, failing when a required validation layer is missing), validation messenger with counters, timeline submission, deferred destruction, sync2 barriers, buffers/images, swapchain |
| renderer | `engine/renderer` | R0: HDR forward pass with fixed R0 lighting, ACES tone map to sRGB, R32 pick IDs bound to the drawn frame, selection outline, offscreen views, readback/capture with provenance, GPU timestamps, matrix-packing compute probe; original HLSL shaders. Public API has no Vulkan types |
| editor | `editor/authoring`, `editor/application`, `editor/panels` | `EditorDocument` (holds the writer lock; every GUI edit goes through `SceneSession`); **`sovereign-editor`**: Dear ImGui hierarchy, viewport with picking, inspector (transform, material, HDR emissive), history with undo/redo, diagnostics (validation, commands, log); editor camera; play/stop in an isolated world; per-event keyboard shortcuts; automation runner; opt-in UI-anchor file for real-input tests |
| CLI | `tools/cli` | **`sovereign-cli`**: `--version`, `scene validate/inspect/new/import/apply/undo/redo/history/diff`, `--project-root` confinement; JSON results; error envelope for pre-evaluation failures |
| sandbox | `games/sandbox` | original sandbox game module: camera flight, one transient probe entity in play/runtime worlds (ADR-004) |
| content | `content/authoring`, `examples/`, `schemas/`, `tests/fixtures/` | First Light (the packet scene, imported with `scene import`), the packet example batch, the owner's schemas, the engine result schema, Axis Lab (asymmetric orientation fixture) |
| tests | `tests/` | `sovereign-tests`, `sovereign-render-tests`, editor automation script, static checks (Unreal-free sources, link dependencies, loaded libraries, runtime has no editor code, headless has no graphics, notices, packet copies, owner-schema conformance) |
| build | `CMakeLists.txt`, `CMakePresets.json`, `cmake/`, `dependencies.lock.json`, `third_party_notices/` | presets, lock-driven FetchContent with revision/SHA-256 verification, shader compilation with a checked compiler, notices |
| scripts | `scripts/` | DXC source build, display launcher, real-input GUI check, failure-path checks (run and configure time), loaded-library check, isolation probe, CLI demo, clean-build, fresh-clone and evidence collection, contract and PNG checks |
| docs | `docs/`, `README.md` | repository guide, conventions (incl. R0 lighting), command protocol, logging/errors, modules, ADR-001..007 and 013..017, GPU procedure, host inventory, this report; `docs/packet/` holds the packet as supplied |

## 3. Commands executed and their results

Logs are under `docs/reports/evidence/logs/` unless another directory is named. Host paths were
rewritten to repository-relative ones (`<repo>`, `<session-scratch>`). Every command was actually run
on the recorded host at the commit in §1 (rows 1-3 at `16f3c53`, whose code is identical; `fcbc52c`
only added those logs).

| # | Command (repository root) | Result | Log |
|---|---|---|---|
| 1 | `scripts/clean_build_evidence.sh`: `rm -rf build/dev-*`, `cmake --preset dev-headless` (fresh fetch), build, `ctest --preset dev-headless` with no display variables | configure 0, build 0 (0 warnings), **18/18 passed** | `clean_dev_headless_{1_configure,2_build,3_test}.log` |
| 2 | same script: `cmake --preset dev-vulkan` (fresh fetch, source-built DXC checked), build, `ctest --preset dev-vulkan` | configure 0, build 0 (0 warnings), **29/29 passed** | `clean_dev_vulkan_{1_configure,2_build,3_test}.log`, `clean_build_summary.txt` |
| 2a | the same script one commit earlier (before the shortcut fix) | **28/29: `vulkan.editor_real_x11_input` failed** (Ctrl+S ignored). Kept as the record of defect §6.13 | `evidence/defects/gui_shortcut_lost_clean_build_test_before_fix.log` |
| 3 | `scripts/fresh_clone_evidence.sh`: `git clone --no-local` of the committed tree into an empty directory, both presets with fresh fetches (`dev-vulkan` with the documented `SOV_DXC_EXECUTABLE` override, since a clone has no `build/tools`) | **18/18** and **29/29 passed**; clone HEAD `fcbc52c` | `fresh_clone_dev_headless.log`, `fresh_clone_dev_vulkan.log` |
| 4 | `unshare -rn` (no network; curl and git both fail inside): touch sources, reconfigure, rebuild and test both presets | exit 0; **18/18** headless, **12/12** static + rendering | `offline_rebuild_both_presets.log` |
| 5 | `cmake --preset dev-headless-asan` (fresh), build, `ctest --preset dev-headless-asan` (`-fsanitize=address,undefined`) | exit 0; **18/18 passed** (all 95 headless tests); **0** sanitizer reports | `asan_{1_configure,2_build,3_test}.log` |
| 6 | `scripts/cli_demo_evidence.sh build/dev-headless/bin docs/reports/evidence/cli` | exit 0; 20 command steps with the expected exit codes, **15/15 checks**, then 10/10 written files conform to the owner's schemas | `cli_demo.log`, `evidence/cli/transcript.md` |
| 7 | `ctest --preset dev-vulkan -V` (fresh artifacts with per-test notes) | **29/29 passed**; 0 `VUID-` or validation messages in the whole log | `dev_vulkan_verbose.log` |
| 8 | `sovereign-runtime --scene <scene saved by the editor> --frames 90 --screenshot ...` | exit 0 | `runtime_opens_editor_saved_scene.log` |
| 9 | `scripts/gui_real_input_check.sh` 10 more times in a row | **10/10 passed** | `gui_real_input_repeated_10x.log` |
| 10 | `scripts/check_contracts.py` on the scenes the editor and the GUI session saved; on the CLI demo's scenes, requests and results | 3/3 and 20/20 conform | `evidence/editor/owner_schema_check.txt`, `evidence/cli/results/19_owner_schema_check.txt` |
| 11 | `python3 -I tools/validate_contract_examples.py`, diffed against the packet's `PACKET_VALIDATION.txt` | 16/16 packet checks; output **byte-identical** to the owner's record | `packet_checker_vs_packet_record.log` |
| 12 | `scripts/configure_failure_paths.sh` (networking disabled) | **5/5** failure cases gave a CMake error with the expected diagnostic | `configure_failure_paths.log`, `evidence/failure_paths/configure/` |
| 13 | `sovereign-runtime --perf-report` at 1280x720 (vsync on, off) and "1920x1080" (vsync off; validation on, off), 720 frames, 120 warm-up | 4 reports written | `perf_*.log`, `evidence/performance/*.json` |
| 14 | `ctest --preset gpu-qualify --output-on-failure` | **exit 8, 7 of 10 failed: expected.** Every device test refused llvmpipe ("CPU/software implementation refused (physical GPU required)"); the 3 that pass are a static check and two fixture-staging steps | `gpu_qualify_on_this_host.log`, `gpu_qualify_device_selection.txt` |
| 15 | `scripts/isolation_check.sh` before and after collection | outside-repository sections identical | `evidence/isolation/{before,after}.txt`, `before_after.diff`, `collect_summary.txt` |
| 16 | `scripts/build_dxc_from_source.sh` (once, previous round) | `libdxcompiler.so: 1.9(dev;1-b106a961)`; ~16 min | not kept (build/scratch); recorded in the lock |

The handoff's commands, exactly as written (transcript: `evidence/cli/transcript.md`):

| Command | Result |
|---|---|
| `sovereign-cli --version` | exit 0: version 0.1.0, commit `fcbc52c (clean)`, lock hash, toolchain, contract versions, "never initializes a graphics device" |
| `sovereign-cli scene validate --scene content/authoring/first_light.scene.json` | exit 0: `valid: "First Light" scene_id=10000000-0000-4000-8000-000000000001 revision=0 entities=12` |
| `sovereign-cli scene apply --scene content/authoring/first_light.scene.json --commands examples/place_banner.command_batch.json` | exit 0, **`status: dry_run`**, revision 0 -> 0, both operations `valid` (none `applied`), planned diff 1 created ("Test banner") + 1 modified (rename to "First Light dais"); scene sha256 identical before and after; no lock, journal or backup file created |
| `sovereign-runtime --scene content/authoring/first_light.scene.json` | runs until closed (interactive). Exercised by `vulkan.runtime_first_light` (`--width 1280 --height 720 --frames 120 --scripted-input --screenshot ...`): passed |

Then, on a copy: a **new committed request** (`dry_run: false`, new key and request id) committed
revision 0 -> 1; the identical retry and a retry with a new `request_id` both returned the recorded
result (`delivery: replayed`, same commit id, file unchanged); changed content under the same key was
rejected (`idempotency_conflict`); a stale `base_revision` was rejected with conflict details; a batch
whose second operation names an unknown entity was rejected whole (operation 0 `valid`, operation 1
`failed`, revision and file unchanged); undo and redo produced revisions 2 and 3 with content equal to
revisions 0 and 1; a committing apply while another writer held the lock exited 3.

## 4. Validation matrix: every ID

### G0

| ID | Check | Status | Evidence |
|---|---|---|---|
| G0-01 | Workspace isolation | **passed** (D) | resolved paths and before/after probes (`evidence/isolation/`): repository `/home/user/SovereignEngine`, build trees inside it, no remote; no production project or Unreal installation on the host; other git repositories (tool installs) unchanged; §7 |
| G0-02 | Actual host inventory | **passed** (D) | `HOST_INVENTORY.md`, `evidence/host/` (OS, CPU, compilers, CMake/Ninja, display, Vulkan, DXC; physical GPU: none) |
| G0-03 | Reproducible dependency selection | **passed** (S) | `dependencies.lock.json` (revisions, archive SHA-256, options); configure prints each revision `(verified)`; a checkout at the wrong revision is refused (`failure_paths/configure/dependency_at_unpinned_revision.log`); `static.license_notices_match_lock`; lock hash in every binary |
| G0-04 | Clean configure/build | **passed** (H, V) | §3 rows 1-5: clean trees, fresh clone, offline rebuild, sanitizers, all exit 0 |
| G0-05 | Display-independent core | **passed** (H, S) | `ctest --preset dev-headless` with `DISPLAY`/`WAYLAND_DISPLAY` unset; `static.headless_sovereign-cli_has_no_graphics`, `static.headless_sovereign-tests_has_no_graphics` (no SDL/Vulkan/ImGui symbols or libraries); headless configure fetches only nlohmann_json + EnTT |
| G0-06 | No Unreal dependency | **passed** (S, V) | source: `static.no_unreal_dependency`; link: `static.<exe>_link_dependencies` for all 5 executables (`NEEDED` = libstdc++, libm, libgcc_s, libc; no RPATH; no Unreal symbols) and `evidence/dependencies/link_time_needed.txt`; run time: `vulkan.loaded_libraries_are_system_only` (runtime and editor each load 52 objects, all from system directories, 17 via `dlopen`); independent execution: §3 rows 1-3 |
| G0-07 | Clear failure paths | **passed** (V, S) | run time, 15/15 (`failure_paths/runtime/failure_paths.json`): missing/malformed scene -> 1; no display, no Vulkan driver, required layer missing, requested device absent, physical GPU required on a CPU-only host, missing shaders after device creation -> 4; launcher without display or Xvfb -> 77 (skipped, never passed). Configure time, 5/5: missing, directory or non-running DXC override, wrong dependency revision, offline without cache |
| G0-08 | Qualification scope | **passed** (D) | §1 "Tested platform"; §6 items 1-5 list every unavailable check |

**G0: complete for Linux x86_64 / GCC 13.3**, the only platform available.

### G1 document and command checks (headless; tests in `tests/unit`, `tests/integration`)

| ID | Check | Status | Evidence |
|---|---|---|---|
| G1-D01 | Scene round trip | **passed** (H) | `scene.roundtrip_is_byte_identical_and_idempotent`, `scene.packet_first_light_reads_and_is_the_canonical_scene`, `store.create_commit_reopen_preserves_document_and_history`, `editor.move_column_undo_material_save_close_reopen` |
| G1-D02 | Schema errors | **passed** (H) | `scene.unknown_schema_version_fails_before_reading`, `scene.unknown_component_types_are_rejected`, `scene.unknown_fields_are_rejected_not_dropped`, `scene.malformed_and_nonfinite_input_fail_with_location`, `scene.value_validation_rejects_bad_numbers`, `scene.missing_required_fields_rejected`, `core.strict_json_rejects_malformed_and_nonfinite`, `core.strict_json_rejects_duplicate_keys`; packet checker negative cases (§3 row 11) |
| G1-D03 | Identity and hierarchy | **passed** (H) | `scene.duplicate_uuid_rejected_duplicate_names_allowed`, `scene.hierarchy_missing_parent_and_cycles_rejected`, `scene.active_camera_must_reference_a_camera`, `commands.reparenting_rejects_cycles_and_inverse_restores_parent` |
| G1-D04 | Math conventions | **passed** (H; also V) | `math.*` (7: right-handed Y-up basis, T*R*S order, inverse/normal matrix, `[x,y,z,w]` quaternion order and Euler display, reversed-Z, Vulkan Y flip, CCW winding), `world.builds_from_document_with_parent_before_child_propagation` (asymmetric parent at (1, 0.5, -2), 45 degree yaw), `world.moving_parent_moves_child_after_update`; on the device: Axis Lab (asymmetric fixture) |
| G1-D05 | Create/update/delete | **passed** (H) | `commands.each_operation_kind_applies`, `commands.create_validates_identity_parent_and_limits`, `commands.delete_respects_dependencies_without_cascade`, `commands.material_set_requires_material_component`, `commands.no_change_edit_does_not_bump_revision`, `scene.diff_reports_field_level_changes` |
| G1-D06 | Atomic batch validation | **passed** (H) | `commands.failed_batch_rolls_back_entirely`, `cli.invalid_batch_rejected_scene_untouched`; CLI demo §5 |
| G1-D07 | Stale revision | **passed** (H) | `commands.stale_revision_rejected_with_conflict_details`, `cli.stale_revision_is_rejected_with_conflict`; `cli/results/09_stale.result.json` (current revision 1, the newer commit listed) |
| G1-D08 | Dry run | **passed** (H) | `commands.dry_run_changes_nothing_and_reserves_nothing`, `commands.dry_run_still_validates`, `cli.example_batch_is_a_dry_run_and_changes_nothing`; CLI demo §2 (file hash, directory listing and revision unchanged) |
| G1-D09 | Duplicate request | **passed** (H) | `commands.duplicate_delivery_returns_recorded_result_without_reapplying`, `store.retry_after_restart_replays_recorded_result`, `cli.committed_request_then_retry_then_changed_content`; CLI demo §4 |
| G1-D10 | Changed request under reused key | **passed** (H) | `commands.reused_key_with_changed_content_is_rejected`, `commands.request_hash_binds_content_not_transport_fields`, `commands.expired_key_reports_explicit_status`; `cli/results/08_changed_content.result.json` |
| G1-D11 | Undo/redo | **passed** (H; also X) | `commands.undo_redo_restore_content_and_revisions_stay_monotonic` (incl. redo cleared by a divergent edit), `commands.undo_requires_current_revision`, `commands.history_rebuilds_undo_stack_from_records`, `cli.undo_redo_and_history`; CLI demo §6; GUI Undo/Redo buttons and Ctrl+Z (`gui_real_input/`) |
| G1-D12 | Save failure | **passed** (H) | `store.failed_atomic_write_preserves_last_valid_scene` (injected replace failure), `core.atomic_write_replaces_and_preserves_on_failure` |
| G1-D13 | Crash/restart durability | **passed** (H) | `store.crash_after_prepare_recovers_as_aborted`, `store.crash_after_replace_recovers_as_committed`, `store.torn_journal_tail_is_ignored`, `store.retry_after_restart_replays_recorded_result`, `store.external_edit_invalidates_history_and_starts_new_epoch`; guarantee stated in ADR-006 |
| G1-D14 | Writer exclusion | **passed** (H; also X) | `store.writer_lock_refuses_second_writer_but_allows_readers`, `store.stale_lock_from_a_killed_writer_is_reclaimed_explicitly` (fork + SIGKILL; the reclaim names the previous holder), `cli.offline_write_refused_while_another_writer_holds_the_lock`, `editor.open_holds_writer_lock_until_closed`; against the real editor process: `gui_real_input/cli_while_editor_open.json` (exit 3, `lock_unavailable`) |
| G1-D15 | Bounded inputs | **passed** (H) | `core.read_file_limit_checked_before_read`, `core.strict_json_enforces_limits`, `commands.batch_parsing_is_strict` (129 operations, 1 MiB + 1 byte, 129-character actor rejected), `commands.create_validates_identity_parent_and_limits`, `commands.scene_mismatch_rejected`, `cli.project_root_confines_paths`, `store.writer_refuses_symlinked_scene_path` |
| G1-D16 | GUI/CLI equivalence | **passed** (H) | `editor.gui_edit_and_cli_edit_produce_identical_authored_state`, `editor.batch_file_applies_through_same_service`; the GUI session's saved scene conforms to the owner's schema |

### G1 rendering and application checks

| ID | Check | Status | Evidence |
|---|---|---|---|
| G1-R01 | Real native rendering | **blocked** | no physical GPU. Native Vulkan rendering of original primitives runs (class V: `rendering.first_light_renders_from_scene_data`, captures), but on llvmpipe, which cannot satisfy this row |
| G1-R02 | Projection/depth/winding | **blocked** (physical GPU required by the handoff) | software run passed: `rendering.axis_lab_orientation_depth_culling_hierarchy_and_pick_ids` (camera direction, occlusion, reversed-Z, back-face culling, hierarchy), `rendering.projection_matches_analytic_fov_and_aspect` (marker edges at the analytic FOV for wide and tall views), plus the H math tests |
| G1-R03 | Selection/picking | **blocked** (physical GPU required by the handoff) | software run passed: `rendering.request_pick_resolves_entity_with_frame_table`, `rendering.pick_resolves_with_the_drawn_snapshot_not_a_stale_table`, `rendering.selection_outline_changes_only_the_selected_edge`; real viewport click picked "Raised platform" (`x11_04_viewport_pick.png`) |
| G1-R04 | Resize/minimize/restore | **passed** (V, X) | `vulkan.runtime_window_lifecycle_stress` (25 minimize/restore/resize steps; buffers/images/allocations 8/3/11 before and after), `rendering.view_resize_and_recreate_cycles_return_to_resource_baseline`, editor automation resize/minimize (no frame rendered while minimized)/restore. Physical display and compositor not tested |
| G1-R05 | GPU lifetime/synchronization | **passed** (V) | validation + synchronization validation on and required in every Vulkan test: 0 errors, 0 warnings (`rendering.zz_validation_summary_after_suite`, runtime and editor summaries; no `VUID-` in `dev_vulkan_verbose.log`); deferred destruction keyed to timeline values; no waivers exist |
| G1-R06 | Save/reopen workflow | **passed** (V, X) | scripted editor run: edit, save, reopen, same hash (`editor/editor_report.json`); real-input session: edits, Ctrl+S, Alt+F4, a new editor process shows the same revision, history and values (`gui_real_input/x11_07_saved.png`, `x11_08_reopened.png`) |
| G1-R07 | Isolated play world | **passed** (H, V) | `play.modifies_only_the_runtime_world`, `editor.edits_disabled_while_playing`; automation: 120 play ticks, stop, document hash equals saved, revision unchanged |
| G1-R08 | Standalone runtime | **passed** (V, S) | `sovereign-runtime` opened the editor-saved and the GUI-saved scenes (§3 row 8, `gui_real_input/runtime_after_gui_session.png`); `static.runtime_has_no_editor_code`; link and load checks (G0-06) |
| G1-R09 | GUI operation is real | **passed** (X, V); human review **not run** | `vulkan.editor_real_x11_input` with real X server input: hierarchy selection, typed position and emissive edits, viewport pick, Delete, History Undo/Redo buttons, Ctrl+Z, Diagnostics validation and command tabs, Ctrl+S, close, reopen; 8 X server captures. Since the shortcut fix (§6.13) it passed every one of its 24 runs (10 manual, 10 in evidence collection, clean build, fresh clone, verbose run, regression run) |
| G1-R10 | Original First Light scene | **passed** (V, D) | the packet's authored scene (12 entities: floor, walkway, platform, rear wall, 6 columns, light strip, camera) rendered from scene data only; background is the scene's clear colour; no backdrop or external image (`rendering.first_light_renders_from_scene_data`: 11 primitives drawn) |
| G1-R11 | Repeated lifecycle | **passed** (H, V) | editor automation: 50 save/play/stop/reopen cycles; buffers 10->10, images 7->7, allocations 17->17, VMA bytes 61,202,344 -> 61,202,344, validation errors 0->0, RSS 238.9 MiB -> 238.9 MiB (after 10) -> 238.7 MiB (end), peak 239.1 MiB; `store.fifty_open_save_close_cycles`, `play.fifty_play_stop_cycles_return_to_baseline` |
| G1-R12 | Actual captures | **passed** (V, X) | every PNG names source, scene name + revision + content hash, device, driver, evidence class, validation state, shader compiler and commit (`captures/PROVENANCE.txt`); X server captures are labelled as such |
| G1-R13 | Baseline performance | **blocked** | no reference machine (ADR-007). The report procedure runs and records everything the row asks for (resolution, content, settings, machine, p50/p95/p99, memory); the four llvmpipe reports in `evidence/performance/` are CPU-rendering numbers, not a baseline |
| G1-R14 | Exit and partial init | **passed** (V) | normal close of runtime and editor with 0 validation messages; 15/15 failure cases (G0-07) incl. failure after device and swapchain creation (missing shaders) for runtime and editor, no hang (120 s limit), no validation or sanitizer output; a failed editor start leaves the scene unchanged and unlocked |
| G1-R15 | Missing GPU honesty | **passed** (V, S) | `gpu-qualify` refuses llvmpipe in every device test (§3 row 14); every result and image carries the software-rendered class; no Null backend exists (ADR-016) |

**G1: NOT complete.** Blocked: G1-R01, G1-R02, G1-R03, G1-R13 (physical GPU / reference machine).
Not run: human review of the editor and First Light. Everything else passed, with the G1-R rows on the
CPU implementation only.

## 5. Captures, scene before/after, request/result, diff

Engine images are readbacks from the engine's own Vulkan images and are **software-rendered (CPU
Vulkan implementation)**. The `x11_*` images were read from the X server by ImageMagick, independently
of the engine. Provenance for each: `docs/reports/evidence/captures/PROVENANCE.txt`.

| Artifact | Path (under `docs/reports/evidence/`) | What it shows |
|---|---|---|
| Runtime | `captures/runtime_first_light.png` | `sovereign-runtime` presented frame of First Light, 1280x720, after 120 frames of scripted camera flight |
| Runtime on editor output | `captures/runtime_editor_saved_scene.png`, `gui_real_input/runtime_after_gui_session.png` | the runtime opening the files the editor saved |
| Editor (automation) | `captures/editor_01_opened.png` ... `editor_06_after_lifecycle.png`, `editor_03_move_undone_viewport.png` | full editor window through pick, move, undo, material edit, save, play, 50 cycles; one viewport readback |
| Editor (real input) | `gui_real_input/x11_01_opened.png` ... `x11_08_reopened.png` | X server captures after typed edits, viewport pick, history, diagnostics, save, reopen |
| Rendering tests | `captures/rendering/axis_lab.png`, `axis_lab_selected.png`, `first_light.png` | offscreen views checked pixel by pixel |
| Scene before/after (CLI) | `cli/scenes/first_light.rev0.before.scene.json`, `rev1.after_commit`, `rev2.after_undo`, `rev3.after_redo` | committed request, undo, redo |
| Command request | `cli/requests/place_banner.commit.command_batch.json` (+ the example as shipped, retry, changed-content, stale and invalid variants) | the new committed request derived from the example |
| Command results | `cli/results/03_example_dry_run.result.json`, `04_commit`, `06_retry_same`, `07_retry_new_request_id`, `08_changed_content`, `09_stale`, `10_invalid`, `11_undo`, `13_redo`, `16_stale_undo` (`.result.json`), `17_locked.error.json` | |
| Diff | `cli/results/05_diff.json` (structural), `cli/results/05_text.diff` (file) | "Test banner" box created; entity `...0103` renamed "Raised platform" -> "First Light dais" |
| Journal | `cli/scenes/first_light.journal.jsonl` | epoch record, then prepare/committed pairs: edit (0->1), undo (1->2), redo (2->3) |
| Editor session | `editor/editor_report.json`, `editor_session.diff.json`, `editor_session.history.json`, `first_light.before_editor.scene.json`, `first_light.saved_by_editor.scene.json` | 32/32 automation steps ok; authored change: Raised platform base colour (0.23, 0.115, 0.045, 1) -> (0.72, 0.40, 0.12, 1), light strip emissive (1.4, 0.45, 0.06) -> (2.8, 0.9, 0.12) |
| Real-input session | `gui_real_input/gui_real_input.log`, `history_after_gui.json`, `diff_after_gui.txt`, `first_light.saved_by_gui.scene.json`, `cli_while_editor_open.json` | journal `[edit, edit, edit, undo, redo, undo]` at revision 6; CLI refused with exit 3 while the editor was open |
| Failure paths | `failure_paths/runtime/`, `failure_paths/configure/` | one log per case with the diagnostic; JSON summaries |
| Dependencies | `dependencies/link_time_needed.txt`, `runtime.loaded_paths.txt`, `editor.loaded_paths.txt`, `*.dlopened.txt` | link-time and run-time library lists |
| Performance | `performance/*.json` | see below |
| Defect record | `defects/` | the failing clean-build log and SDL event excerpt behind §6.13 |

Performance reports (procedure demonstration only; llvmpipe on 4 vCPU; Debug build; 600 measured
frames after 120 warm-up; First Light, 12 instances (11 authored primitives + the sandbox probe),
840 triangles; frame time is wall time between frame starts):

| Report | Frame p50 / p95 / p99 (ms) | GPU p50 / p95 / p99 (ms) | Renderer CPU work p50 (ms) | RSS / VMA peak |
|---|---|---|---|---|
| 1280x720, vsync, validation | 9.28 / 11.02 / 12.11 | 7.50 / 9.05 / 9.80 | 0.68 | 175 MiB / 29 MiB |
| 1280x720, no vsync, validation | 9.29 / 11.65 / 12.73 | 7.54 / 9.46 / 10.49 | 0.69 | 176 MiB / 29 MiB |
| 1918x1055, no vsync, validation | 20.27 / 24.08 / 26.93 | 17.14 / 20.65 / 22.69 | 0.89 | 213 MiB / 46 MiB |
| 1918x1055, no vsync, no validation | 19.18 / 23.19 / 26.00 | 16.97 / 20.78 / 23.00 | 0.22 | 164 MiB / 46 MiB |

Notes: 1920x1080 was requested; on the 1920x1080 virtual screen the window manager gave 1918x1055,
and the reports record the actual size. Xvfb has no vertical blank, so FIFO presentation does not
throttle and vsync on/off match. "GPU" here is llvmpipe executing on the CPU.

## 6. Known defects, unsupported features, deviations

**Blocked or not verified**

1. **No physical GPU** (ADR-001, ADR-013). G1-R01, R02, R03 and R13 are blocked; every other G1-R
   result is software-rendered. A CPU implementation cannot reveal driver-specific bugs, memory-type
   or tiling differences, real present-mode behaviour, or performance.
2. **No performance baseline** (G1-R13, ADR-007). The perf reports in `evidence/performance/` exercise
   the procedure on llvmpipe; their numbers are CPU-rendering times and are not a baseline.
3. **Windows and macOS were never built.** Windows code paths (`LockFileEx`, `MoveFileExW`, prebuilt
   DXC selection, `K32GetProcessMemoryInfo`) are uncompiled. The windowed CTest entries use bash.
4. **No human review** of the editor, the runtime or First Light on a real monitor. G1-R09 passed on
   scripted real input; a person has not used it.
5. Real-input coverage (G1-R09): hierarchy clicks, typed inspector values (Ctrl+click, type, Enter),
   viewport pick, Delete, History buttons, Ctrl+Z, Diagnostics tabs, Ctrl+S, Alt+F4 and a second editor
   process were real X server input. **Not** driven by input events: dragging inspector fields, colour
   pickers, menus, F5 play/stop, editor camera flight, window resize by mouse. Play/stop, resize and
   minimize were exercised by the automation runner and the runtime stress test instead.

**Deviations (each recorded)**

6. **Contracts replaced** (ADR-017). The first round reconstructed the packet files from the TDD
   (ADR-014). When the owner's packet arrived, the engine moved to its contracts: scene/component field
   names, full-replacement `transform.set`/`material.set`, required `dry_run`, 128 operations, actor
   length, no lights; the packet's First Light replaced the reconstructed hall. Packet files are
   byte-identical copies (`static.packet_files_verbatim`).
7. **R0 fixed lighting**: contract v1 has no lights, so every scene uses one fixed key light + ambient
   (conventions.md). **Alpha** is stored and edited but rendered opaque, with a load warning.
8. **Engine-defined result schema** and a separate **CLI error envelope** for failures before a batch is
   evaluated (lock unavailable, I/O). Documented in `docs/architecture/command_protocol.md`.
9. **Third-party rights question, stopped and worked around rather than decided:** the prebuilt Linux
   DXC package's `LICENSE-MS.txt` says "solely for use on Windows" while its `ReleaseNotes.md` assigns
   LICENSE-LLVM to all files. DXC is built from the pinned NCSA source on non-Windows hosts; the
   prebuilt package needs `-DSOV_DXC_ALLOW_PREBUILT=ON` (ADR-015). **Needs Matthew's decision.**
10. The TDD §10.3 RHI contract is not a separate API; the boundary is the renderer's public API, and
    there is no Null RHI (ADR-016). TDD §5.1 `engine/rhi/null` and G2+ module directories not created.
11. CLI path confinement is opt-in (`--project-root`); agents should always be given it.
12. The committed CLI demonstration ran on a copy of First Light, so the repository's canonical scene
    stays at revision 0, which the packet example targets. The dry run ran on the canonical file itself.

**Defects found and fixed during this assignment** (none open)

13. **Keyboard shortcuts could be lost** (found while regenerating this evidence: the first clean-build
    run of `vulkan.editor_real_x11_input` failed on Ctrl+S; repeated runs lost Ctrl+Z or Ctrl+S in
    about one run in three). SDL event logging showed Ctrl down, Z down, Ctrl up, Z up with the last
    three in one frame; the editor sampled ImGui's per-frame key state after the panels, by which time
    the Ctrl release had been applied, so the chord was dropped. Slow frames make it likely; a quick tap
    at a low frame rate on real hardware can hit it too. Shortcuts are now decided per SDL key event
    with that event's modifier state (`EditorApp::ShortcutFor`). Since the fix it has passed all 24 of its runs,
    including 10 consecutive runs during evidence collection (`logs/gui_real_input_repeated_10x.log`). Before-fix logs: `evidence/defects/`.
14. Inspector material edits typed into **Emissive** never committed: the commit check looked at the
    last ImGui item (the colour swatch drawn after the field) instead of the field. Found by the
    real-input GUI test; fixed by capturing the field's state immediately after it is drawn.
15. Pick requests bound the entity table **when requested**, not when the frame that drew the pixels was
    recorded; a scene change between request and draw could map an ID to the wrong entity. Now bound
    at draw time; regression test `rendering.pick_resolves_with_the_drawn_snapshot_not_a_stale_table`.
16. Reclaiming a writer lock left by a killed writer was silent. The lock now records the previous
    holder and the store reports "reclaimed a stale writer lock left by ..."
    (`store.stale_lock_from_a_killed_writer_is_reclaimed_explicitly`, G1-D14).
17. A nonexistent `SOV_DXC_EXECUTABLE` passed configure and failed later at the first shader; configure
    now fails with the path, and also when the compiler cannot run (G0-07).
18. The renderer's per-frame CPU time included fence/acquire/present waits; split into
    `renderer_cpu_work` and `renderer_blocked` before any performance report was recorded.
19. Earlier in G1 (previous round, all still fixed): dry-run operations labelled `applied`; a reconfigure
    mislabelling the shader compiler as user-supplied; absolute host paths in capture provenance; the GUI
    check hanging when the editor failed to open; Vulkan validation findings (independent blend, upload
    and readback hazards, present stage mask, unused vertex attribute). No validation waiver exists.
20. Test-tooling defects caught before use: a test of mine held an `EntityData*` across an edit
    (heap-use-after-free found by ASan); the failure-path checker initially matched diagnostics against
    its own log header, which could pass vacuously. Both fixed before any evidence was recorded.

**Environment note (not an engine defect)**

21. Under XTEST on Xvfb, SDL 3.2 delivers a zero-length synthetic click twice (it reads XInput2 events
    from the master and slave devices and drops the copy only when press and release are not
    back-to-back). `xev` sees one press. The GUI test holds the button 80 ms like a physical mouse.
    Physical mice were not tested.

**Known limitations kept in scope**

22. Durability is single-scene, single-writer, local file system (ADR-006); no autosave or
    recovery-snapshot UI; idempotency retention uses wall-clock time.
23. The validation layer on this host is 1.3.275 (Ubuntu 24.04); newer layers may report more.
24. R0 shading is deliberately simple: no shadows, PBR, textures, transparency or GI (G2+).

## 7. Production isolation, spending and publication

- All work stayed inside `/home/user/SovereignEngine` (a new repository) and container-local
  packages. `scripts/isolation_check.sh` ran before and after evidence collection
  (`evidence/isolation/`): no ProjectVelkorran, Velkorran, Origins, Battles of the Reach, GASPALS,
  Cinemafy, Holofy, Grungify or Unreal installation exists on this host; the only other git
  repositories are tool installs (`/opt/nvm`, `/opt/rbenv`, ...), unchanged between the two probes.
  "The MMO" has no name to search for; no unrelated repository was opened for writing.
  No production save was read or written.
- No Epic/Unreal code, headers, modules, tools or assets: checked in source
  (`static.no_unreal_dependency`), at link time (`static.<exe>_link_dependencies`) and at run time
  (`vulkan.loaded_libraries_are_system_only`). First Light and Axis Lab are original primitives with
  original shaders.
- Dependencies came from upstream repositories/releases at pinned revisions with their notices
  (`third_party_notices/`, ADR-003); a wrong revision is refused (G0-07 evidence).
- **No remote exists** (`git remote -v` is empty); nothing was pushed, published or released; no licence
  chosen (ADR-009); no telemetry; no paid service or cloud resource used beyond the session's container.
  Ubuntu packages came from the free archive; the Python `jsonschema` validator was preinstalled.
- Not introduced: D3D12, Metal, OpenGL, SDL_GPU, a second renderer, C++ modules, Rust, C#, a scripting
  VM, networking or RPC, a plugin marketplace, a job system.

## 8. Next recommended task

**Run GPU qualification on a physical-GPU machine Matthew authorizes**, following
`docs/reports/GPU_TEST_PROCEDURE.md`, and replace the blocked and software-rendered G1-R results with
physical-GPU results. No new features first.

Acceptance check: on that machine, from a clean checkout at this report's commit,
`ctest --preset gpu-qualify --output-on-failure` reports **100% tests passed, 0 skipped**; every test
log names the physical GPU as the selected device; `vulkan.rendering` reports 0 validation errors and
0 warnings with synchronization validation on; `runtime_first_light.png` provenance reads
`EvidenceClass: gpu-rendered (physical GPU)`; three `--perf-report` runs at 1920x1080 are recorded
(G1-R13); and Matthew opens the editor and runtime on First Light and records his review (G1-R09).
Only then mark G1 complete.

*Execution stops here, after this report, as the handoff requires. Nothing continues in the background.*
