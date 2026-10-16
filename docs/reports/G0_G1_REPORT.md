# G0 / G1 report: First Ignition

Date: 2026-10-16. Branch `research/first-ignition`. Author: Claude (Caelis role), for Matthew.

**Bottom line.** G0 is complete on the recorded host (Linux x86_64, GCC 13). G1 is **implemented and
software-tested but NOT complete**: the host has no physical GPU, so every item that needs "native
rendering on a named physical GPU" is **BLOCKED**, and the items that need Matthew's review are
**NOT RUN**. All graphics results below were produced by the real engine on Mesa's CPU Vulkan
implementation (llvmpipe/lavapipe) and are labelled **software-rendered**. There is no Null backend, no
mocked renderer and no fabricated image anywhere in this repository.

Evidence classes used (TDD §1.3): **H** headless-tested, **S** static check, **V** Vulkan executed on
the CPU implementation (software-rendered), **G** GPU-tested on a physical GPU, **R** human-reviewed,
**D** document/record.

---

## 1. Identity: commit, host, toolchain, GPU, driver, lock, backend

| Item | Value |
|---|---|
| Code commit the evidence binaries were built from | `384eaea603d398b5f650cacd63644bfa94b10bd2` (tree clean at build time; `--version` prints `(clean)`) |
| Report commit | the commit that adds this file (child of the above; adds only `docs/`) |
| Dependency lock | `dependencies.lock.json` sha256 `c2ad80e256bb24a21da9321f9c562a1a2cb1d2393845987f493e1e80cb5a1065` (compiled into every binary) |
| Host | managed cloud container, KVM guest; Ubuntu 24.04.5 LTS x86_64, kernel 6.18.44; 4 vCPU Intel Xeon @ 2.10 GHz; 15 GiB RAM (`docs/reports/HOST_INVENTORY.md`) |
| Toolchain | GCC 13.3.0, CMake 3.28.3, Ninja 1.11.1, GNU ld 2.42, Python 3.13.16, git 2.43.0; C++20, `-Werror` on engine code |
| GPU | **none** (no display-class PCI device, no `/dev/dri`) |
| Vulkan device used | `llvmpipe (LLVM 20.1.2, 256 bits)`, `PHYSICAL_DEVICE_TYPE_CPU`, Mesa 25.2.8-0ubuntu0.24.04.4, API 1.4.318, conformance 1.3.1.1; loader 1.3.275; `VK_LAYER_KHRONOS_validation` 1.3.275 with synchronization validation |
| Backend | native Vulkan 1.3 core (dynamic rendering, synchronization2, timeline semaphores) via volk + VMA; one graphics queue; 2 frame contexts |
| Shader compiler | DXC v1.8.2505.1 **built from source** @ `b106a961d092` (NCSA), `-spirv -fspv-target-env=vulkan1.3 -Zpc -HV 2021 -WX -O3` (ADR-015) |
| Evidence class of every image | **software-rendered (CPU Vulkan implementation)** (ADR-013) |
| Untested platforms | Windows, macOS, any physical GPU, any physical display: **not run**, no claim made |

## 2. What was built (modules and files)

Everything in this repository is new in this assignment; nothing outside it was modified. 172 tracked
files excluding evidence; about 14,000 lines of engine-owned C++20/HLSL plus 2,600 lines of tests.
Dependency direction and boundaries: `docs/architecture/modules.md`.

| Module | Path | Contents |
|---|---|---|
| core | `engine/core` | `Result`/`Error` codes, canonical UUIDs, SHA-256, logging, strict JSON (duplicate keys, depth, size, non-finite rejected) + canonical writer, durable file I/O (atomic replace, durable append, `flock`/`LockFileEx` writer lock), math (Vec3/Quat/Mat4, reversed-Z projection), original PNG encoder with tEXt provenance, build identity |
| world | `engine/world` | `SceneDocument` + reflection descriptors (the single field allowlist), strict reader / canonical writer, structural validation, field-level diff, content hash, provenance label; EnTT-backed `World` behind a handle API, parent-before-child propagation, render snapshot, primitive geometry |
| commands | `engine/commands` | typed operations, strict batch parser, staged whole-batch validation with inverse ops, `SceneSession` (revisions, idempotency ledger bound to the content hash, undo/redo), `SceneStore` (writer lock, JSONL journal, prepare/backup/replace/verify/commit, recovery) |
| runtime | `engine/runtime` | 60 Hz fixed-step `Simulation`, `GameModule` seam; `app/` = **`sovereign-runtime`** |
| platform | `engine/platform` | SDL3 window, events, input action map |
| rhi/vulkan | `engine/rhi/vulkan` | device selection (refuses CPU devices on request), validation messenger with counters, timeline submission, deferred destruction, sync2 barriers, buffers/images, swapchain |
| renderer | `engine/renderer` | R0: HDR forward pass, ACES tone map to sRGB, R32 pick IDs, selection outline, offscreen views, readback/capture with provenance, matrix-packing compute probe; original HLSL shaders |
| editor | `editor/authoring`, `editor/application`, `editor/panels` | `EditorDocument` (holds the writer lock; every GUI edit goes through `SceneSession`); **`sovereign-editor`**: Dear ImGui hierarchy, viewport, inspector, history, diagnostics; editor camera; play/stop; automation runner |
| CLI | `tools/cli` | **`sovereign-cli`**: `--version`, `scene validate/inspect/new/apply/undo/redo/history/diff`, `--project-root` confinement |
| sandbox | `games/sandbox` | original sandbox game module (ADR-004) |
| content | `content/`, `examples/`, `schemas/`, `tests/fixtures/` | First Light (S1), Axis Lab (S0), the command batches that build them, the example batch, wire schemas |
| tests | `tests/` | `sovereign-tests` (9 suites, 91 tests), `sovereign-render-tests` (8 tests), editor automation script, static checks; 19 CTest entries in `dev-vulkan` |
| build | `CMakeLists.txt`, `CMakePresets.json`, `cmake/`, `dependencies.lock.json`, `third_party_notices/` | presets, lock-driven FetchContent with revision/SHA-256 checks, shader compilation, static checks, notices |
| scripts | `scripts/` | scene batch generator, DXC source build, display launcher, real-input GUI check, CLI demo, clean-build and evidence collection, PNG checks |
| docs | `docs/`, `AGENTS.md`, `README.md` | conventions, command protocol, logging/errors, modules, ADR-001..007 and 013..016, validation matrix, GPU procedure, host inventory, this report |

## 3. Commands executed and their results

All logs are under `docs/reports/evidence/logs/`; host-specific absolute paths were rewritten to
repository-relative ones. Every command below was actually run on the recorded host at the commit in §1.

| # | Command (from the repository root) | Result | Log |
|---|---|---|---|
| 1 | `rm -rf build/dev-*` then `cmake --preset dev-headless` (fresh fetch) | exit 0 | `clean_dev_headless_1_configure.log` |
| 2 | `cmake --build --preset dev-headless` | exit 0, 0 warnings | `clean_dev_headless_2_build.log` |
| 3 | `ctest --preset dev-headless` with `DISPLAY`/`WAYLAND_DISPLAY` unset | **13/13 passed** | `clean_dev_headless_3_test.log` |
| 4 | `cmake --preset dev-vulkan` (fresh fetch; source-built DXC selected) | exit 0 | `clean_dev_vulkan_1_configure.log` |
| 5 | `cmake --build --preset dev-vulkan` | exit 0, 0 warnings | `clean_dev_vulkan_2_build.log` |
| 6 | `ctest --preset dev-vulkan --output-on-failure` | **19/19 passed** (software-rendered) | `clean_dev_vulkan_3_test.log` |
| 7 | `unshare -rn` (no network): touch sources, reconfigure + rebuild + test both presets | exit 0; 13/13 headless, 4/4 static+rendering | `offline_rebuild_both_presets.log` |
| 8 | `cmake --preset dev-headless-asan`, build, `ctest --preset dev-headless-asan` | exit 0; **13/13 passed**; 0 sanitizer reports (`-fsanitize=address,undefined`, GCC) | `asan_1_configure.log`, `asan_2_build.log`, `asan_3_test.log` |
| 9 | `scripts/cli_demo_evidence.sh build/dev-headless/bin docs/reports/evidence/cli` | exit 0, 19 command steps each with the expected exit code; 15/15 checks | `cli_demo.log`, `evidence/cli/transcript.md` |
| 10 | `ctest --preset dev-vulkan -V` (fresh artifacts with per-test notes) | **19/19 passed** | `dev_vulkan_verbose.log` |
| 11 | `sovereign-runtime --scene <scene saved by the editor> --frames 90 --screenshot ...` | exit 0 | `runtime_opens_editor_saved_scene.log` |
| 12 | `ctest --preset gpu-qualify --output-on-failure` | **exit 8, 5 of 8 failed: expected.** All 5 device tests refuse llvmpipe ("CPU/software implementation refused (physical GPU required)"); the 3 that pass are the static check and two fixture-staging steps | `gpu_qualify_on_this_host.log`, `gpu_qualify_device_selection.txt` |
| 13 | `git clone` of this repository into an empty directory, then the three `dev-headless` commands | 13/13 passed; then `dev-vulkan` in the same clone with `-DSOV_DXC_EXECUTABLE=<built dxc>` (the documented override): 19/19 passed | `fresh_clone_dev_headless.log`, `fresh_clone_dev_vulkan.log` |
| 14 | `scripts/build_dxc_from_source.sh` (once) | exit 0; `libdxcompiler.so: 1.9(dev;1-b106a961)`; ~16 min compile | not kept (build/scratch); result recorded in the lock |

The handoff's four CLI commands, exactly as written (transcript: `docs/reports/evidence/cli/transcript.md`):

| Command | Result |
|---|---|
| `sovereign-cli --version` | exit 0: version 0.1.0, commit, lock hash, toolchain, contract versions |
| `sovereign-cli scene validate --scene content/authoring/first_light.scene.json` | exit 0: `valid: "First Light" ... revision=1 entities=96` |
| `sovereign-cli scene apply --scene content/authoring/first_light.scene.json --commands examples/place_banner.command_batch.json` | exit 0, **`status: dry_run`**, revision 1 -> 1, every operation `valid` (none `applied`); scene file sha256 identical before/after; no lock, journal or backup file created |
| `sovereign-runtime --scene content/authoring/first_light.scene.json` | runs until closed (interactive). Exercised by CTest `vulkan.runtime_first_light`, which adds `--width 1280 --height 720 --frames 120 --scripted-input --screenshot ...`: passed |

## 4. Validation matrix results

Rows are `docs/VALIDATION_MATRIX.md` (reconstructed, ADR-014). "PASS (V)" means passed on the CPU
Vulkan implementation; where a row needs **G**, the row is BLOCKED regardless.

### G0

| ID | Result | Evidence |
|---|---|---|
| G0-01 host recorded | **PASS** (D) | `HOST_INVENTORY.md`, `evidence/host/` |
| G0-02 isolated repository | **PASS** (D) | §7 |
| G0-03 dev-headless configure/build/test | **PASS** (H) | §3 rows 1-3 |
| G0-04 dev-vulkan configure/build/test | **PASS** (V) | §3 rows 4-6 |
| G0-05 headless needs no display/SDL/Vulkan | **PASS** (S, H) | `static.headless_sovereign-cli_has_no_graphics`, `static.headless_sovereign-tests_has_no_graphics`; configure fetches only nlohmann_json + EnTT; tests ran with no display variables |
| G0-06 pinned dependencies | **PASS** (S) | configure logs print each revision `(verified)` / archive sha256; `FETCHCONTENT_UPDATES_DISCONNECTED=ON` |
| G0-07 notices preserved and checked | **PASS** (S) | `static.license_notices_match_lock`: 8 primary + 13 additional notice files |
| G0-08 clean bootstrap | **PASS** (H, V) | fresh build trees with fresh fetches (§3 rows 1-6) and a fresh `git clone` (row 13) |
| G0-09 offline rebuild | **PASS** (H, V) | §3 row 7: network namespace with no connectivity (curl and git both fail inside it) |
| G0-10 `--version` identity | **PASS** (H) | `cli.version_reports_build_identity`; `evidence/cli/results/01_version.txt` |
| G0-11 unit test target | **PASS** (H) | `sovereign-tests`, 9 suites |
| G0-12 log/error conventions | **PASS** (D, H) | `docs/architecture/logging_and_errors.md`; `commands.schema_contracts_match_engine_codes_and_operations` |
| G0-13 schema fixtures match engine | **PASS** (H) | `scene.schema_files_match_reflection_descriptors`, schema contract test |
| G0-14 decision records | **PASS** (D) | `docs/decisions/` |
| G0-15 no Unreal dependency | **PASS** (S) | `static.no_unreal_dependency` |
| G0-16 warnings as errors; sanitizers | **PASS** (H) | 0 warnings with `-Werror`; ASan+UBSan run (§3 row 8). MSVC/Clang sanitizers not run |
| G0-17 untested platforms recorded | **PASS** (D) | §1, §6 |

**G0: complete for Linux x86_64 / GCC 13.3** (the only platform available).

### G1: scene, math, commands, persistence (headless)

| ID | Result | Evidence (tests in `tests/unit`, `tests/integration`) |
|---|---|---|
| G1-SCN-01 UUID identity | **PASS** (H) | `core.uuid_canonical_roundtrip`, `core.uuid_rejects_non_canonical_forms`, `scene.duplicate_uuid_rejected_duplicate_names_allowed` |
| G1-SCN-02 strict validation, no silent loss | **PASS** (H) | `scene.unknown_fields_are_rejected_not_dropped`, `scene.unknown_component_types_are_rejected`, `scene.unknown_schema_version_fails_before_reading` |
| G1-SCN-03 values, required fields, limits | **PASS** (H) | `scene.value_validation_rejects_bad_numbers`, `scene.missing_required_fields_rejected`, `scene.active_camera_must_reference_a_camera`, `core.strict_json_*` |
| G1-SCN-04 byte-identical round trip | **PASS** (H) | `scene.roundtrip_is_byte_identical_and_idempotent` |
| G1-SCN-05 hierarchy, parent-before-child | **PASS** (H) | `scene.hierarchy_missing_parent_and_cycles_rejected`, `world.builds_from_document_with_parent_before_child_propagation`, `world.moving_parent_moves_child_after_update` |
| G1-SCN-06 runtime handles | **PASS** (H) | `world.handles_are_generation_and_world_checked` |
| G1-MATH-01 basis, units, TRS, quaternions | **PASS** (H) | `math.*` (7 tests) |
| G1-MATH-02 reversed-Z, Y flip, CCW winding | **PASS** (H) | `math.reversed_z_*`, `math.vulkan_y_flip_*`, `math.front_face_winding_*`, `world.primitive_triangles_wound_ccw_from_outside` |
| G1-MATH-03 shader packing on device | PASS (V); **BLOCKED** (G) | `rendering.shader_matrix_packing_matches_cpu_math` (constant and structured buffers) |
| G1-CMD-01 five operation kinds | **PASS** (H) | `commands.each_operation_kind_applies` |
| G1-CMD-02 one revision per batch | **PASS** (H) | `commands.batch_increments_revision_once_regardless_of_operation_count`, `commands.no_change_edit_does_not_bump_revision` |
| G1-CMD-03 failed-batch rollback | **PASS** (H) | `commands.failed_batch_rolls_back_entirely`, `cli.invalid_batch_rejected_scene_untouched`; demo §5: op 0 `valid`, op 1 `failed`, file unchanged |
| G1-CMD-04 stale revision + conflict details | **PASS** (H) | `commands.stale_revision_rejected_with_conflict_details`, `cli.stale_revision_is_rejected_with_conflict`; demo `09_stale.result.json` |
| G1-CMD-05 duplicate delivery | **PASS** (H) | `commands.duplicate_delivery_returns_recorded_result_without_reapplying`, `store.retry_after_restart_replays_recorded_result`; demo `06_*`, `07_*` (same commit id, `delivery: replayed`) |
| G1-CMD-06 changed content under same key | **PASS** (H) | `commands.reused_key_with_changed_content_is_rejected`, `commands.request_hash_binds_content_not_transport_fields`; demo `08_changed_content.result.json` (`idempotency_conflict`) |
| G1-CMD-07 expired key | **PASS** (H) | `commands.expired_key_reports_explicit_status` |
| G1-CMD-08 dry run default, no writes, no reservation | **PASS** (H) | `commands.dry_run_*`, `cli.example_batch_is_a_dry_run_and_changes_nothing`; demo §2 |
| G1-CMD-09 dependency/reference rules | **PASS** (H) | `commands.delete_respects_dependencies_without_cascade`, `commands.create_validates_identity_parent_and_limits`, `commands.reparenting_rejects_cycles_and_inverse_restores_parent`, `commands.material_set_requires_material_component`, `commands.scene_mismatch_rejected` |
| G1-CMD-10 CLI refuses offline write while editor holds lock | **PASS** (H, V) | `store.writer_lock_refuses_second_writer_but_allows_readers`, `cli.offline_write_refused_while_another_writer_holds_the_lock`, `editor.open_holds_writer_lock_until_closed`; against the **real running editor**: `vulkan.editor_real_x11_input` (CLI exit 3, `evidence/gui_real_input/cli_while_editor_open.json`) |
| G1-CMD-11 limits and path policy | **PASS** (H) | `commands.batch_parsing_is_strict`, `cli.project_root_confines_paths`, `store.writer_refuses_symlinked_scene_path`, `core.read_file_limit_checked_before_read` |
| G1-UNDO-01 undo/redo restore content, revisions monotonic | **PASS** (H) | `commands.undo_redo_restore_content_and_revisions_stay_monotonic`, `commands.undo_requires_current_revision`, `cli.undo_redo_and_history`; demo: rev 3 content == rev 1, rev 4 content == rev 2 |
| G1-UNDO-02 history survives reopen | **PASS** (H) | `commands.history_rebuilds_undo_stack_from_records`, `store.create_commit_reopen_preserves_document_and_history` |
| G1-PER-01 atomic save, failed write keeps last valid | **PASS** (H) | `core.atomic_write_replaces_and_preserves_on_failure`, `store.failed_atomic_write_preserves_last_valid_scene` |
| G1-PER-02 journal recovery | **PASS** (H) | `store.crash_after_prepare_recovers_as_aborted`, `store.crash_after_replace_recovers_as_committed`, `store.torn_journal_tail_is_ignored`, `store.external_edit_invalidates_history_and_starts_new_epoch` |
| G1-PER-03 50 open/save/close cycles | **PASS** (H) | `store.fifty_open_save_close_cycles` |
| G1-PER-04 editor save/close/reopen | **PASS** (H, V) | `editor.move_column_undo_material_save_close_reopen`, `editor.unsaved_edits_are_not_written`; editor automation `save`/`reopen`/`expect document_hash_equals_saved` |
| G1-EQ-01 GUI edit == CLI edit | **PASS** (H) | `editor.gui_edit_and_cli_edit_produce_identical_authored_state` (byte-identical files) |
| G1-EQ-02 one shared service | **PASS** (H) | `editor.batch_file_applies_through_same_service`; module graph (`EditorDocument` -> `SceneSession`) |

### G1: editor, play, runtime, renderer

| ID | Result | Evidence |
|---|---|---|
| G1-ED-01 editor panels and workflow | PASS (V); **human review NOT RUN** | automation transcript `evidence/editor/editor_report.json` (all steps ok, 0 failures); captures `editor_01..06`; real X11 input check |
| G1-ED-02 viewport picking | PASS (V); **BLOCKED** (G) | `rendering.request_pick_resolves_entity_with_frame_table`; automation `pick` (pixel (477, 267) -> pick index -> Altar UUID); a real XTEST mouse click selected the Altar |
| G1-ED-03 UUID selection | **PASS** (H) | `editor.selection_is_by_uuid_and_cleared_when_entity_deleted` |
| G1-PLAY-01 play changes only the runtime world | **PASS** (H, V) | `play.modifies_only_the_runtime_world`; automation `expect` after stop: revision 5, not dirty, document hash equals saved |
| G1-PLAY-02 50 play/stop cycles | **PASS** (H, V) | `play.fifty_play_stop_cycles_return_to_baseline`; editor `cycles` n=50: buffers 10->10, images 7->7, VMA allocations 17->17, validation errors 0->0, 1 live world |
| G1-PLAY-03 edits refused while playing; clock | **PASS** (H) | `editor.edits_disabled_while_playing`, `play.fixed_step_clock_caps_catch_up` |
| G1-RT-01 runtime has no editor code | **PASS** (S) | `static.runtime_has_no_editor_code` |
| G1-RT-02 runtime opens and presents First Light, and the editor-saved scene | PASS (V); **BLOCKED** (G) | `vulkan.runtime_first_light`; §3 row 11; captures `runtime_first_light.png`, `runtime_editor_saved_scene.png` |
| G1-RT-03 resize/minimize/restore/shutdown | PASS (V); **BLOCKED** (G) | `vulkan.runtime_window_lifecycle_stress`: 25 lifecycle steps (resize, minimize, restore), buffers/images/allocations 8/3/11 -> 8/3/11, exit 0; `rendering.view_resize_and_recreate_cycles_return_to_resource_baseline`; editor resize/minimize/restore steps |
| G1-GPU-01 Vulkan 1.3 device + swapchain + validation | PASS (V); **BLOCKED** (G) | `rendering.device_startup_with_validation` |
| G1-GPU-02 depth, culling, orientation, hierarchy | PASS (V); **BLOCKED** (G) | `rendering.axis_lab_orientation_depth_culling_hierarchy_and_pick_ids` |
| G1-GPU-03 selection outline | PASS (V); **BLOCKED** (G) | `rendering.selection_outline_changes_only_the_selected_edge` |
| G1-GPU-04 view resize resource baseline | PASS (V); **BLOCKED** (G) | `rendering.view_resize_and_recreate_cycles_return_to_resource_baseline` |
| G1-GPU-05 zero validation messages | PASS (V); **BLOCKED** (G) | `rendering.zz_validation_summary_after_suite`: layer ON, synchronization validation ON, 0 errors, 0 warnings; editor: 0 errors, 0 warnings; runtime: 0/0 |
| G1-GPU-06 native rendering on a named physical GPU | **BLOCKED** | no physical GPU on this host; `gpu-qualify` refuses llvmpipe (§3 row 12) |
| G1-GPU-07 baseline on reference machine | **BLOCKED / NOT RUN** | ADR-007 |
| G1-SCENE-01 Axis Lab | **PASS** (H, V) | `tests/fixtures/axis_lab.*`; `captures/rendering/axis_lab.png` |
| G1-SCENE-02 First Light as editable scene data | **PASS** (H, V) | built by `content/source/first_light.build.command_batch.json` through the command service; validates; 82 primitive instances, 4,232 triangles (G1 limit: 100 / 100,000); `captures/rendering/first_light.png` |
| G1-SCENE-03 hall reviewed by Matthew | **NOT RUN** (R) | awaiting review |
| G1-EVID-01 captures name their source | **PASS** (V) | `captures/PROVENANCE.txt` (every PNG: source, scene revision + content hash, device, evidence class, validation state, shader compiler, commit) |
| G1-EVID-02 proof set | PASS for software-rendered evidence; physical-GPU validation log **BLOCKED** | §5 |
| G1-ISO-01 isolation, no spending/publication | **PASS** (D) | §7 |
| G1-ISO-02 no excluded technology | **PASS** (S, D) | §7 |

**G1: NOT complete.** Blocked items: G1-MATH-03, ED-02, RT-02, RT-03, GPU-01..07 (physical GPU).
Not run: ED-01 and SCENE-03 human review. Everything else passed.

## 5. Artifacts: captures, scenes, request/result, diff

All images are **software-rendered (CPU Vulkan implementation)** readbacks from the engine's own
images, except the three `x11_*` files, which ImageMagick read from the X server (independent of the
engine). Provenance for each: `docs/reports/evidence/captures/PROVENANCE.txt`.

| Artifact | Path (under `docs/reports/evidence/`) | What it shows |
|---|---|---|
| Runtime image | `captures/runtime_first_light.png` | `sovereign-runtime` presented frame of First Light, 1280x720 |
| Runtime on editor output | `captures/runtime_editor_saved_scene.png` | the runtime opening the scene file the editor saved (revision 5, Altar material changed) |
| Editor images | `captures/editor_01_opened.png` ... `editor_06_after_lifecycle.png` | full editor window (hierarchy, viewport, inspector, history, diagnostics) through pick, move, undo, material edit, save, play, lifecycle |
| Editor viewport | `captures/editor_03_move_undone_viewport.png` | viewport view readback |
| Real-input captures | `gui_real_input/x11_editor_after_pick.png`, `..._after_delete.png`, `..._after_undo_save.png` | X server captures after a real XTEST click, Delete, Ctrl+Z, Ctrl+S |
| Rendering-test images | `captures/rendering/axis_lab.png`, `axis_lab_selected.png`, `first_light.png` | offscreen views checked pixel-by-pixel by the tests |
| Scene before/after (CLI) | `cli/scenes/first_light.rev1.before.scene.json`, `rev2.after_commit`, `rev3.after_undo`, `rev4.after_redo` | the committed request, undo and redo |
| Command request | `cli/requests/place_banner.commit.command_batch.json` (+ retry, changed-content, stale, invalid variants) | the new committed request derived from the example (`dry_run: false`, new key and request id) |
| Command results | `cli/results/03_example_dry_run.result.json`, `04_commit.result.json`, `06_retry_same.result.json`, `08_changed_content.result.json`, `09_stale.result.json`, `10_invalid.result.json`, `11_undo.result.json`, `13_redo.result.json`, `17_locked.result.json` | |
| Diff | `cli/results/05_diff.json` (structural), `cli/results/05_text.diff` (file) | 2 entities created (banner + rod), Monolith Seam `emissive_strength` changed |
| Journal | `cli/scenes/first_light.journal.jsonl` | epoch, then prepare/committed pairs: edit (rev 1->2), undo (2->3), redo (3->4) |
| Editor session | `editor/editor_report.json`, `editor/editor_session.diff.json`, `editor/editor_session.history.json`, `editor/first_light.before_editor.scene.json`, `editor/first_light.saved_by_editor.scene.json` | automation transcript, device, validation and resource counters; the authored change; history |
| Real-input session | `gui_real_input/gui_real_input.log`, `history_after_gui.json`, `diff_after_gui.txt`, `editor_stdout.log` | journal `[edit, undo]`, revision 3, content equal to original |
| Validation logs | `logs/dev_vulkan_verbose.log` (per-test device, validation and resource notes), `logs/gpu_qualify_on_this_host.log` | software-rendered validation only |

## 6. Defects, unsupported features, deviations

**Blocked / not verified**

1. No physical GPU: G1 GPU qualification blocked (ADR-001, ADR-013). Software rendering cannot reveal
   driver-specific bugs, memory-type or tiling differences, present-mode behaviour, or performance.
2. No performance baseline (ADR-007). Frame times on llvmpipe are not reported as performance.
3. Windows and macOS never built. Windows code paths (`LockFileEx`, `MoveFileExW`, prebuilt DXC
   selection) are uncompiled. The windowed CTest entries use bash scripts.
4. No human review of the editor or the hall (ED-01, SCENE-03).
5. Real-input coverage: the viewport click, Delete, Ctrl+Z, Ctrl+S and window close used real X server
   input (XTEST on Xvfb). Inspector widget edits, hierarchy clicks, menus and the F5 play toggle were
   **not** exercised with input events; the automation runner performed those operations by calling the
   same `EditorDocument`/editor functions directly.

**Deviations from the TDD or handoff (each recorded in an ADR)**

6. Reconstructed packet files: `AGENTS.md`, `docs/VALIDATION_MATRIX.md`, `schemas/command_batch` and
   `command_result`, First Light and the example batch were not supplied and were rebuilt from the TDD
   (ADR-014). Matthew's originals, if they exist, win.
7. **Third-party rights question, stopped and worked around rather than decided:** the prebuilt Linux DXC
   package's `LICENSE-MS.txt` says "solely for use on Windows" while its `ReleaseNotes.md` assigns
   LICENSE-LLVM to all files. DXC is now built from the pinned NCSA source on non-Windows hosts; the
   prebuilt package needs `-DSOV_DXC_ALLOW_PREBUILT=ON` (ADR-015). Earlier development builds used the
   prebuilt package; **all evidence in this report was regenerated with the source-built compiler.**
   Needs Matthew's decision.
8. TDD §10.3 RHI contract (typed generation-checked handles for buffers/textures/pipelines/command lists)
   is not a separate API; the boundary is the renderer's public API, and there is no Null RHI (ADR-016).
9. TDD §5.1 proposed `engine/rhi/null`; not created (ADR-016). G2+ module directories not created.
10. The TDD names the G0 proof `g0-report.md`; the handoff asked for one combined `G0_G1_REPORT.md` (this file).
11. CLI path confinement is opt-in (`--project-root`); without it the CLI is a local operator tool
    acting on the paths given. Agents should always be given `--project-root`.
12. The committed CLI demonstration ran on a copy of First Light (`evidence/cli/`), so the repository's
    canonical scene stays at revision 1, which the example batch targets. The dry run ran on the canonical
    file itself. Same code path either way.

**Defects found and fixed during this assignment** (none open)

13. Command results labelled operations `applied` in dry runs and rejected batches. Now `valid`;
    `applied` only in committed results (the handoff: "Do not report a dry run as an applied edit").
14. A reconfigure could mislabel the shader compiler as "user-supplied" and keep a previous DXC,
    because the resolved path was force-written into the user-override cache variable.
15. Capture provenance recorded absolute host paths; now scene file name + revision + content hash.
16. `gui_real_input_check.sh` hung (to the 900 s CTest timeout) when the editor exited before showing a
    window; it now fails within a second with the editor's error.
17. Vulkan validation findings fixed earlier in G1 (no waivers exist): blend attachments differing
    without `independentBlend` (VUID-VkPipelineColorBlendStateCreateInfo-pAttachments-00605),
    write-after-read hazards on uploads, the present-after-write stage mask, write-after-write on
    readback buffers, and an unused-vertex-attribute performance warning.

**Record correction**

18. The message of commit `f58aab0` says "CLI demo 17/17 checks"; the demo has 15 checks (all pass, as
    recorded in `evidence/cli/transcript.md`). The commit message miscounted; nothing else was affected.

**Known limitations kept in scope**

19. Durability guarantee is single-scene, single-writer, local file system (ADR-006). No autosave or
    recovery-snapshot UI. Idempotency retention uses wall-clock time.
20. The validation layer on this host is 1.3.275 (Ubuntu 24.04); newer layers may report more.
21. G1 shading is simple (one directional light, ACES tone map, emissive); no shadows, PBR or GI (G2).

## 7. Isolation, spending and publication

- Work stayed inside `/home/user/SovereignEngine` (a new repository) and container-local packages.
  ProjectVelkorran, Origins, Battles of the Reach, the MMO, GASPALS, Cinemafy, Holofy, Grungify, Unreal
  installations, production saves and unrelated repositories were **not accessed or modified**; none
  were present on this host.
- No Epic/Unreal code, headers, modules, tools or assets (`static.no_unreal_dependency`). First Light
  and Axis Lab are original primitives with original shaders.
- Dependencies came from their upstream repositories/releases at pinned revisions, with notices
  (`third_party_notices/`, ADR-003).
- **No remote was created** (`git remote -v` is empty), nothing was pushed or published, no release,
  no licence chosen (ADR-009), no telemetry. No paid service or paid cloud resource was bought or used
  beyond the session's own container. Packages came from the free Ubuntu archive.
- Not introduced: D3D12, Metal, OpenGL, SDL_GPU, a second renderer, C++ modules, Rust, C#, a scripting
  VM, networking or RPC, a plugin marketplace, a job system.

## 8. Next recommended task

**Run GPU qualification on a physical-GPU machine Matthew authorizes**, following
`docs/reports/GPU_TEST_PROCEDURE.md`, and turn the BLOCKED rows into PASS/FAIL. No new features first.

Acceptance check: on that machine, from a clean checkout at this report's commit,
`ctest --preset gpu-qualify --output-on-failure` reports **100% passed, 0 skipped**; the logs name the
physical GPU as the selected device; `vulkan.rendering` reports 0 validation errors and 0 warnings with
synchronization validation on; `runtime_first_light.png` provenance reads
`EvidenceClass: gpu-rendered (physical GPU)`; then Matthew opens the editor and runtime on First Light
and records his review (G1-ED-01, G1-SCENE-03). Only then mark G1 complete.

*Execution stops here, after this report, as the handoff requires. Nothing continues in the background.*
