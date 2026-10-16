# Validation matrix: G0 and G1

> **Reconstructed** (ADR-014). The handoff names `docs/VALIDATION_MATRIX.md`, but it was not supplied.
> Rows come from TDD v0.1 §1.3, §3, §4.2, §10.4, §11.3-11.5, §19.4-19.5, §21.1-21.5, §22 (G0/G1
> acceptance and proof) and the First Ignition handoff's "validation that cannot be skipped".
> If Matthew's original exists, it wins.

**Evidence classes** (TDD §1.3): `H` headless-tested, `S` static check, `V` Vulkan executed (on this
host: software-rendered CPU Vulkan, ADR-013), `G` GPU-tested on a named **physical** GPU, `R` human-reviewed
by Matthew, `D` document/record. A row needing `G` or `R` cannot pass from `V`, `H` or `D` evidence.

Results live in `docs/reports/G0_G1_REPORT.md` §4, row by row: PASS, FAIL, BLOCKED or NOT RUN.

## G0: reproducible foundation

| ID | Requirement | Needs | Verified by |
|---|---|---|---|
| G0-01 | Actual host, OS, CPU, RAM, GPU, driver, toolchain recorded; nothing assumed | D | `docs/reports/HOST_INVENTORY.md`, `evidence/host/` |
| G0-02 | Independent repository; no production project, Unreal install or unrelated repo touched | D | git history; report §7 |
| G0-03 | `cmake --preset dev-headless`, `cmake --build --preset dev-headless`, `ctest --preset dev-headless` succeed | H | `evidence/logs/dev_headless_*` |
| G0-04 | Same three commands for `dev-vulkan` succeed | V | `evidence/logs/dev_vulkan_*` |
| G0-05 | Headless targets need no display, SDL or Vulkan (configure fetches none; binaries link none; run with no display) | S, H | `static.headless_*_has_no_graphics`; headless run with `DISPLAY` unset |
| G0-06 | Dependencies pinned: revision verified for git sources, SHA-256 for archives; no floating versions | S | `dependencies.lock.json`; configure log lines |
| G0-07 | Notices preserved and hash-checked against lock and upstream | S | `static.license_notices_match_lock` |
| G0-08 | Clean checkout bootstraps from documented prerequisites (fresh build tree) | H, V | `evidence/logs/clean_*` |
| G0-09 | Populated dependency cache rebuilds without network | H, V | `evidence/logs/offline_rebuild_*` |
| G0-10 | `sovereign-cli --version` reports version, commit, lock hash, toolchain, contracts | H | `cli.version_reports_build_identity`; `evidence/cli/01_version.txt` |
| G0-11 | Unit test target exists and runs | H | `sovereign-tests` suites |
| G0-12 | Log and error conventions documented and used | D, H | `docs/architecture/logging_and_errors.md`; `commands.schema_contracts_match_engine_codes_and_operations` |
| G0-13 | Schema fixtures exist and match the engine | H | `scene.schema_files_match_reflection_descriptors`, schema contract test |
| G0-14 | Decision records for toolchain/backend/dependencies | D | `docs/decisions/` |
| G0-15 | No Unreal headers, libraries, tools or content required | S | `static.no_unreal_dependency` |
| G0-16 | Warnings as errors on engine code; sanitizers run where supported | H | `-Werror` in every build log; `dev-headless-asan` run |
| G0-17 | Untested platforms recorded without inference | D | report §1, §6 |

## G1: First Light

### Scene, identity, math (TDD §6, §8)

| ID | Requirement | Needs | Verified by |
|---|---|---|---|
| G1-SCN-01 | Persistent identity is canonical UUID; nil/non-canonical rejected; names are labels | H | `core.uuid_*`, `scene.duplicate_uuid_rejected_duplicate_names_allowed` |
| G1-SCN-02 | Strict validation: unknown fields/components/versions are errors, never silently dropped | H | `scene.unknown_fields_are_rejected_not_dropped`, `scene.unknown_component_types_are_rejected`, `scene.unknown_schema_version_fails_before_reading` |
| G1-SCN-03 | Values, required fields, limits and active camera validated | H | `scene.value_validation_*`, `scene.missing_required_fields_rejected`, `scene.active_camera_must_reference_a_camera`, `core.strict_json_*` |
| G1-SCN-04 | Canonical round trip: parse then write is byte-identical; save/reopen preserves meaning | H | `scene.roundtrip_is_byte_identical_and_idempotent` |
| G1-SCN-05 | Hierarchy: missing parent and cycles rejected; transforms propagate parent-before-child | H | `scene.hierarchy_missing_parent_and_cycles_rejected`, `world.builds_from_document_with_parent_before_child_propagation`, `world.moving_parent_moves_child_after_update` |
| G1-SCN-06 | Runtime handles are generation/world checked and never persisted | H | `world.handles_are_generation_and_world_checked` |
| G1-MATH-01 | Right-handed, +Y up, meters; M = T*R*S with column vectors; quaternion order | H | `math.*` |
| G1-MATH-02 | Reversed-Z projection, Y flip in projection only, CCW front faces | H | `math.reversed_z_*`, `math.vulkan_y_flip_*`, `math.front_face_winding_*`, `world.primitive_triangles_wound_ccw_from_outside` |
| G1-MATH-03 | Shader matrix packing matches CPU math on the device | V, **G** | `rendering.shader_matrix_packing_matches_cpu_math` |

### Commands, history, persistence (TDD §11.3-11.5, §19)

| ID | Requirement | Needs | Verified by |
|---|---|---|---|
| G1-CMD-01 | create, rename, transform, material, delete operations | H | `commands.each_operation_kind_applies` |
| G1-CMD-02 | One revision increment per non-empty committed batch; no-change does not bump | H | `commands.batch_increments_revision_once_*`, `commands.no_change_edit_does_not_bump_revision` |
| G1-CMD-03 | Invalid batch rolls back entirely (staged whole-batch validation) | H | `commands.failed_batch_rolls_back_entirely`, `cli.invalid_batch_rejected_scene_untouched` |
| G1-CMD-04 | Stale `base_revision` rejected with current revision and conflict details | H | `commands.stale_revision_rejected_with_conflict_details`, `cli.stale_revision_is_rejected_with_conflict` |
| G1-CMD-05 | Duplicate delivery returns the recorded result and applies nothing | H | `commands.duplicate_delivery_*`, `store.retry_after_restart_replays_recorded_result`, CLI demo |
| G1-CMD-06 | Key reuse with changed content rejected; key bound to content hash, not transport fields | H | `commands.reused_key_with_changed_content_is_rejected`, `commands.request_hash_binds_content_not_transport_fields` |
| G1-CMD-07 | Expired key reports an explicit status | H | `commands.expired_key_reports_explicit_status` |
| G1-CMD-08 | Dry run is the default, validates, changes and reserves nothing | H | `commands.dry_run_*`, `cli.example_batch_is_a_dry_run_and_changes_nothing`, CLI demo (file hash before/after) |
| G1-CMD-09 | Deletion dependency, parent, cycle, component and scene-identity rules | H | `commands.delete_respects_dependencies_without_cascade`, `commands.create_validates_*`, `commands.reparenting_*`, `commands.material_set_requires_material_component`, `commands.scene_mismatch_rejected` |
| G1-CMD-10 | Writer lock: offline CLI write refused while the editor holds the scene | H, V | `store.writer_lock_*`, `cli.offline_write_refused_*`, `editor.open_holds_writer_lock_until_closed`, `vulkan.editor_real_x11_input` |
| G1-CMD-11 | Input limits and path policy (`..`, absolute, symlink escapes) | H | `commands.batch_parsing_is_strict`, `cli.project_root_confines_paths`, `store.writer_refuses_symlinked_scene_path`, `core.read_file_limit_checked_before_read` |
| G1-UNDO-01 | Undo/redo restore content; revisions keep increasing; base revision required | H | `commands.undo_redo_*`, `commands.undo_requires_current_revision`, `cli.undo_redo_and_history` |
| G1-UNDO-02 | History survives reopen (rebuilt from journal records) | H | `commands.history_rebuilds_undo_stack_from_records`, `store.create_commit_reopen_preserves_document_and_history` |
| G1-PER-01 | Atomic save; a failed write keeps the last valid scene | H | `core.atomic_write_*`, `store.failed_atomic_write_preserves_last_valid_scene` |
| G1-PER-02 | Crash recovery via journal at each commit step; torn tail; external edits | H | `store.crash_after_*`, `store.torn_journal_tail_is_ignored`, `store.external_edit_*` |
| G1-PER-03 | 50 open/save/close cycles (TDD §21.5) | H | `store.fifty_open_save_close_cycles` |
| G1-PER-04 | Editor save/close/reopen preserves authored state; unsaved edits not written | H, V | `editor.move_column_undo_material_save_close_reopen`, `editor.unsaved_edits_are_not_written`, editor automation |
| G1-EQ-01 | Same edit via GUI service and via CLI yields byte-identical authored state | H | `editor.gui_edit_and_cli_edit_produce_identical_authored_state` |
| G1-EQ-02 | GUI and CLI share one service (batch through the editor uses the same path) | H | `editor.batch_file_applies_through_same_service`; module graph |

### Editor, play, runtime (TDD §11, §5.3)

| ID | Requirement | Needs | Verified by |
|---|---|---|---|
| G1-ED-01 | Editor: hierarchy, viewport, inspector, selection, save/reopen, undo history, errors visible | V, **R** | editor automation + captures; `vulkan.editor_real_x11_input` |
| G1-ED-02 | Viewport picking resolves the clicked entity by UUID | V, **G** | `rendering.request_pick_resolves_entity_with_frame_table`, automation `pick`, real-input click |
| G1-ED-03 | Selection is UUID-based and survives deletes correctly | H | `editor.selection_is_by_uuid_and_cleared_when_entity_deleted` |
| G1-PLAY-01 | Play uses a separate world; authored document unchanged after stop | H, V | `play.modifies_only_the_runtime_world`, automation `expect` after stop |
| G1-PLAY-02 | 50 play/stop cycles return to baseline (TDD §21.5) | H, V | `play.fifty_play_stop_cycles_return_to_baseline`; automation `cycles` n=50 (resources + validation) |
| G1-PLAY-03 | Edits are refused while playing; fixed-step clock caps catch-up | H | `editor.edits_disabled_while_playing`, `play.fixed_step_clock_caps_catch_up` |
| G1-RT-01 | `sovereign-runtime` is separate and links no editor/UI/command code | S | `static.runtime_has_no_editor_code` |
| G1-RT-02 | Runtime loads First Light and presents frames | V, **G** | `vulkan.runtime_first_light` + capture |
| G1-RT-03 | Resize, minimize/restore, shutdown without leaks or validation errors | V, **G** | `vulkan.runtime_window_lifecycle_stress`, `rendering.view_resize_and_recreate_*`, automation lifecycle steps |

### Renderer and GPU (TDD §10, §21.3)

| ID | Requirement | Needs | Verified by |
|---|---|---|---|
| G1-GPU-01 | Real Vulkan 1.3 device and swapchain with validation + synchronization validation | V, **G** | `rendering.device_startup_with_validation` |
| G1-GPU-02 | Depth testing, culling, orientation, hierarchy on the device | V, **G** | `rendering.axis_lab_orientation_depth_culling_hierarchy_and_pick_ids` |
| G1-GPU-03 | Selection outline affects only the selected silhouette | V, **G** | `rendering.selection_outline_changes_only_the_selected_edge` |
| G1-GPU-04 | View resize/recreate returns to the resource baseline | V, **G** | `rendering.view_resize_and_recreate_cycles_return_to_resource_baseline` |
| G1-GPU-05 | Zero validation errors and warnings across the suite | V, **G** | `rendering.zz_validation_summary_after_suite`, exit code 6 checks in editor/runtime |
| G1-GPU-06 | **Native rendering on a named physical GPU** | **G** | `ctest --preset gpu-qualify` (refuses CPU devices), `docs/reports/GPU_TEST_PROCEDURE.md` |
| G1-GPU-07 | First baseline on the recorded reference machine (TDD §21.2-21.3, ADR-007) | **G** | GPU procedure §4 |

### Scenes, evidence, isolation

| ID | Requirement | Needs | Verified by |
|---|---|---|---|
| G1-SCENE-01 | S0 Axis Lab fixture from original primitives | H, V | `tests/fixtures/axis_lab.*`; rendering tests |
| G1-SCENE-02 | S1 First Light: walkway, columns, raised platform; bronze, dark red, charcoal, restrained emissive; editable scene data built through the command service; within G1 content limits | H, V | `content/source/first_light.build.command_batch.json`, `scene validate`, `rendering.first_light_renders_from_scene_data` |
| G1-SCENE-03 | Hall reviewed for readability and mood | **R** | Matthew |
| G1-EVID-01 | Captures name their real source, device and evidence class | V | PNG tEXt chunks (`scripts/png_provenance.py`) |
| G1-EVID-02 | Proof set: editor image, runtime image, scene before/after/diff, request/result, test logs, GPU validation log, reproducible commands | V, D | `docs/reports/evidence/` |
| G1-ISO-01 | No production edits, Unreal code, public remote, release, licence choice, spending or paid cloud use | D | report §7 |
| G1-ISO-02 | No excluded tech (D3D12, Metal, second renderer, C++ modules, Rust, C#, scripting VM, networking, marketplace, job system) | S, D | source tree; report §7 |
