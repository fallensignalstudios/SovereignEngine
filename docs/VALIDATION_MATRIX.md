# Validation matrix

**Authority:** Sovereign Engine TDD v0.1.  
**Current engine implementation status:** Not implemented in this packet. All engine checks below begin as **not run**.  
**Reporting:** Passed / failed / blocked / not run, with platform and evidence path. A requirement is not satisfied by a test that has only been written.

## G0: foundation

| ID | Required check | Acceptance evidence |
|---|---|---|
| G0-01 | Workspace isolation | Resolved repository/build paths outside production; before/after status check for any referenced production repo |
| G0-02 | Actual host inventory | OS, architecture, compilers, CMake/Ninja, display, Vulkan, DXC, physical GPU/driver when available |
| G0-03 | Reproducible dependency selection | Immutable revisions/checksums, notices, selected options, lock hash |
| G0-04 | Clean configure/build | Actual logs and exit results from documented presets |
| G0-05 | Display-independent core | Native unit/scene/command tests execute without creating a display or GPU device |
| G0-06 | No Unreal dependency | Source/build/link/runtime dependency inspection and clean independent execution |
| G0-07 | Clear failure paths | Missing prerequisite produces diagnostic and nonzero result, not silent fallback success |
| G0-08 | Qualification scope | Report names tested platforms and all unavailable checks separately |

## G1: authored state and commands

| ID | Required check | Acceptance evidence |
|---|---|---|
| G1-D01 | Scene round trip | Save/load preserves IDs, transforms, hierarchy, camera, material fields, and scene revision |
| G1-D02 | Schema errors | Unknown versions/types/fields, bad types, malformed JSON, nonfinite numbers fail with location |
| G1-D03 | Identity and hierarchy | Duplicate UUID, missing parent/camera, hierarchy cycle rejected |
| G1-D04 | Math conventions | Asymmetric fixture confirms basis, units, local/world composition and quaternion convention |
| G1-D05 | Create/update/delete | Supported commands change only intended entities/fields; dependency-aware delete |
| G1-D06 | Atomic batch validation | Invalid operation after a valid one leaves content and revision unchanged |
| G1-D07 | Stale revision | Request based on old revision rejected with current revision and conflict information |
| G1-D08 | Dry run | Returns planned diff; no file write, revision change, or successful-commit key reservation |
| G1-D09 | Duplicate request | Same successful request/key returns prior result and creates no duplicate entity/effect |
| G1-D10 | Changed request under reused key | Rejected; neither hidden overwrite nor accidental new action |
| G1-D11 | Undo/redo | Authored state restored; revisions increase monotonically; redo invalidated after divergent edit |
| G1-D12 | Save failure | Injected write/replace failure preserves last committed scene and reports failure |
| G1-D13 | Crash/restart durability | Recovery validates document/journal; no acknowledged lost commit or duplicated retry within documented guarantee |
| G1-D14 | Writer exclusion | Offline CLI refuses conflicting writer; stale lock recovery is explicit and safe |
| G1-D15 | Bounded inputs | Oversized batch/payload, escaped path, invalid references rejected before harmful mutation |
| G1-D16 | GUI/CLI equivalence | Same semantic edit from both clients yields equivalent serialized authored state |

## G1: graphics and editor/runtime

| ID | Required check | Acceptance evidence |
|---|---|---|
| G1-R01 | Real native rendering | Named physical GPU/driver, active Vulkan backend, original primitives rendered |
| G1-R02 | Projection/depth/winding | Fixture verifies camera direction, aspect/FOV, occlusion, reversed-Z and front-face convention |
| G1-R03 | Selection/picking | Selected rendered primitive resolves to the correct stable entity; no stale frame mapping |
| G1-R04 | Resize/minimize/restore | Correct aspect/targets, no zero-size loop or stale resource use |
| G1-R05 | GPU lifetime/synchronization | Validation/synchronization logs clean or narrowly documented accepted waiver; no resource use after retirement |
| G1-R06 | Save/reopen workflow | Human or scripted editor run edits scene, closes app, reopens same state |
| G1-R07 | Isolated play world | Play-time movement/spawns/changes do not alter authored file or editor-world values |
| G1-R08 | Standalone runtime | Separate executable opens saved scene without editor UI or production/Unreal dependencies |
| G1-R09 | GUI operation is real | Hierarchy, inspector edits, material change, undo/redo, diagnostics visibly usable |
| G1-R10 | Original First Light scene | Authored primitive scene, not hard-coded backdrop or external generated image |
| G1-R11 | Repeated lifecycle | At least 50 open/save/close and 50 play/stop cycles; settled resource counts and memory reported |
| G1-R12 | Actual captures | Editor/runtime screenshots identify scene hash, commit, device, and capture source |
| G1-R13 | Baseline performance | Resolution/content/settings/machine recorded; p50/p95/p99 and memory reported; provisional misses stay visible |
| G1-R14 | Exit and partial init | Normal close and failed initialization unwind cleanly without crash/deadlock |
| G1-R15 | Missing GPU honesty | Unsupported physical device/display produces blocked status; Null/software results cannot satisfy R01 |

## Gate completion rule

G0 requires G0-01 through G0-08. G1 requires the G1-D and G1-R checks above, plus its G0 prerequisites. Partial headless success is valuable but does not qualify G1.

Implementation may divide these checks into more granular tests. It may not remove a requirement or weaken its meaning without a recorded, owner-approved change. A missing feature can be reported as missing; it must not be converted into an 'optional' pass in the report.

Performance thresholds in the TDD are provisional and fixture-specific. Record misses and their impact. Do not claim broad hardware/game performance from this small sample.

## Later-gate priority checks

**G2:** Parser bounds and unsupported extensions; material/color-space/axis correctness; license quarantine; incremental cook keys; failed reimport retains valid data; shader reload failure; clean cooked-only runtime; no source/editor paths; real static-scene visual review.

**G3:** Capsule/slope/step/corner collision; bind-pose and clip playback; root-motion ownership; hit-window sweep/deduplication; shield spillover; exactly-once death/reward; resource/cooldown failure; stale AI requests; checkpoint reset; audio/HUD lifecycle; standalone combat package.

**G4:** Authored intent versus resolved plan versus recorded take; unsupported capability and unreachable target; motor/camera ownership; interruption cleanup; bake fidelity; scrubbing without gameplay side effects; skip/final-state equivalence; manual-edit preservation; motion database rights and reproducibility.

**G5:** Save compatibility/corruption; recovery; streaming readiness and cancel/unload; multi-file authoring durability if introduced; memory residency; actual game UI/input/accessibility; alternate-machine packaging; representative mini-mission regression.

**G6:** Game-specific acceptance and migration inventory. Only Matthew authorizes adoption.
