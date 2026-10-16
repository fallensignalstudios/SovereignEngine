# Command protocol, history and durability

Wire contracts: `schemas/command_batch.schema.json`, `schemas/command_result.schema.json` (both
reconstructed from TDD §19.2-19.3, ADR-014). Implementation: `engine/commands`. The schemas describe
shape only; the guarantees below are enforced by `cmd::SceneSession` and `cmd::SceneStore` and
proven by `tests/unit/test_commands.cpp`, `tests/unit/test_store.cpp`, `tests/integration/test_cli.cpp`
and `tests/integration/test_editor.cpp`.

## One authoring path

```
sovereign-editor panels ─┐
sovereign-cli scene ... ─┼─> editor::EditorDocument / cmd::SceneStore ─> cmd::SceneSession ─> SceneDocument
future agent transport ──┘                                                   (staging, revisions,
                                                                               idempotency, history)
```

Nothing else mutates a `SceneDocument`. The editor's ECS world is rebuilt from the document after
each accepted edit. Play mode copies the document into a separate world and never writes back.

## Batch semantics (`sovereign.command_batch` v1)

A batch carries `request_id`, `actor`, `scene_id`, `base_revision`, `idempotency_key`, optional
`dry_run` (**default `true`**) and 1-512 operations (`entity.create`, `entity.rename`, `transform.set`,
`material.set`, `entity.delete`). Requests are limited to 1 MiB and JSON nesting depth 12; actors to 64 bytes and keys to
128 bytes; duplicate JSON keys, non-finite numbers and unknown fields are rejected.

Evaluation order, first failure wins:

1. **Idempotency** (`actor`, `idempotency_key`) within the 7-day retention window. Same normalized
   content returns the recorded result verbatim with `delivery: "replayed"` and applies nothing.
   Different content: `idempotency_conflict`. Older than retention: `idempotency_expired`
   (explicit, never silently treated as new).
2. **Scene identity**: `scene_id` must match (`scene_mismatch`).
3. **Revision**: `base_revision` must equal the current revision, else `stale_revision` with a
   `conflict` object: `base_revision`, `current_revision`, `commits_since_base` (commit id, kind, actor,
   revisions, label, touched entities) and `history_complete`.
4. **Staged validation**: operations are applied in array order to a staged copy. Later operations
   see earlier ones (create-then-parent works). The first failing operation is `failed`; the rest are
   `not_evaluated`. The fully staged document must pass `ValidateStructure`. Any failure rejects the
   whole batch, leaving document, revision, files, history and idempotency ledger untouched.
5. **Outcome**: `dry_run` returns the diff and changes nothing (no journal entry, no key reservation,
   no lock taken by the CLI). An empty effective change returns `no_change`. Otherwise one commit:
   revision + 1 regardless of operation count.

The **content hash** that keys are bound to is SHA-256 over canonical JSON of
`{protocol_version, scene_id, base_revision, normalized operations}`. `request_id` (transport identity)
and `dry_run` are excluded, so a retry may carry a new `request_id`.

Reference rules: no cascading deletes; deleting an entity with children or the scene's active camera
fails with `missing_dependency`; parents must exist (possibly created earlier in the batch);
re-parenting checks cycles; `material.set` needs a material component (`component_missing`).

## History: undo and redo

Every committed transaction is a `TransactionRecord` (commit id, kind, actor, revisions, forward and
inverse operations, request hash). Undo/redo are history-service calls, not batch operations:

- They require the current revision as `--base-revision` (else `stale_revision`).
- They produce a **new** revision; the counter never rewinds. Undo of revision 2 at revision 2 gives
  revision 3 whose content equals revision 1.
- The undo and redo stacks are **derived** from the journal records on open, so history survives
  close/reopen (`commands.history_rebuilds_undo_stack_from_records`, `store.create_commit_reopen_...`).
- A new edit clears the redo stack.

## Writer lock

`<scene>.scene.json.lock` is held with `flock` (POSIX) / `LockFileEx` (Windows) by any writer:
`sovereign-editor` for its whole session, `sovereign-cli` only for the duration of a committing
`apply`/`undo`/`redo`. The holder writes a small JSON description (process, pid, time). A second writer
gets `lock_unavailable` and the CLI exits **3**. Readers (validate, inspect, history, dry runs) take no
lock. Proven by `store.writer_lock_refuses_second_writer_but_allows_readers`,
`cli.offline_write_refused_while_another_writer_holds_the_lock` and, against the real editor process,
`vulkan.editor_real_x11_input`.

## Durability (ADR-006)

Files next to `<scene>.scene.json`: `.lock`, `.journal` (JSON Lines), `.bak` (previous committed bytes).

Commit:

1. append + fsync `{"type":"prepare", seq, revision_before, revision_after, scene_sha256_after, records}`
2. atomic write of `.bak` with the current scene bytes
3. atomic write of the scene (temp file in the same directory, fsync, rename, directory fsync)
4. re-read and verify SHA-256
5. append + fsync `{"type":"committed", seq}`

Recovery on open (writer mode): a trailing `prepare` without a marker is **rolled forward** if the
scene file has `revision_after` and `scene_sha256_after`, **aborted** if it still has `revision_before`,
otherwise the journal is declared **inconsistent** (history and idempotency are not trusted; a new
epoch starts). A torn final line is ignored. Read-only opens compute the same decision in memory and
write nothing. An external edit of the scene file (hash differs from the last committed) starts a new
epoch with an empty history.

Guarantee actually provided in G1: single-scene, single-writer, crash-consistent commits with
exactly-once application per (actor, key) inside the retention window, on local file systems that
honour fsync and rename. **Not** provided: autosave, multi-scene transactions, network file systems,
multi-writer merges, recovery snapshots with a visible diff (TDD §11.5 later gate).

Tests: `store.crash_after_prepare_recovers_as_aborted`, `store.crash_after_replace_recovers_as_committed`,
`store.failed_atomic_write_preserves_last_valid_scene`, `store.torn_journal_tail_is_ignored`,
`store.retry_after_restart_replays_recorded_result`, `store.fifty_open_save_close_cycles`,
`store.external_edit_invalidates_history_and_starts_new_epoch`, `core.atomic_write_replaces_and_preserves_on_failure`.

## Path safety (TDD §19.4)

`--project-root DIR` confines every path argument to `DIR` after normalization; `..` escapes,
absolute paths outside the root and symlink escapes are rejected (`cli.project_root_confines_paths`).
Writers refuse a scene path that is itself a symlink (`store.writer_refuses_symlinked_scene_path`).
Scene files are size-checked before reading (32 MiB, JSON depth 16, 10,000 entities, hierarchy
depth 64); batches at 1 MiB.

## Exit codes

| Code | `sovereign-cli` | `sovereign-runtime` | `sovereign-editor` |
|---|---|---|---|
| 0 | success: valid, committed, dry run, replayed duplicate, no change | ok | ok |
| 1 | rejected: invalid document/batch, conflict, stale revision | scene invalid | cannot open scene |
| 2 | usage error | usage | usage |
| 3 | scene writer lock unavailable | - | - |
| 4 | I/O failure | platform/graphics failure | platform/graphics failure |
| 5 | internal error | - | automation step failed |
| 6 | - | Vulkan validation errors seen | Vulkan validation errors seen |
| 7 | - | resource growth under `--stress-window` | - |
