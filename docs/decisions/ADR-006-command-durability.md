# ADR-006: Journal-backed single-scene commits

Status: Accepted. Date: 2026-10-16. Details: `docs/architecture/command_protocol.md` §Durability.

TDD §11.3: exactly-once behaviour needs a journal tying request identity to the scene commit; two
unrelated JSON writes are not enough.

## Decision

Each scene has a JSON Lines journal next to it. A commit appends+fsyncs a `prepare` record (which
contains the full transaction records, including idempotency key and request hash), writes a backup,
atomically replaces the scene, verifies its hash, then appends `committed`. On open, an unterminated
`prepare` is rolled forward or aborted by comparing the scene file with the recorded revisions and
hashes; anything else is declared inconsistent and starts a new epoch rather than guessing.

## Guarantee (what exists, nothing more)

Single scene, single writer (OS file lock), crash-consistent, exactly-once per (actor, idempotency key)
within 7 days, on local file systems honouring fsync/rename. History (undo/redo) survives reopen.
No autosave, recovery snapshot UI, multi-scene transactions or network file systems in G1.

## Evidence

Crash injection at each commit step (`store.crash_after_prepare_recovers_as_aborted`,
`store.crash_after_replace_recovers_as_committed`), injected write failure
(`store.failed_atomic_write_preserves_last_valid_scene`), torn journal tail, retry after restart, 50
open/save/close cycles. Power-loss behaviour of the real disk was not tested (cannot be on this host).
