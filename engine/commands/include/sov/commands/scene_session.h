#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "sov/commands/command_batch.h"
#include "sov/commands/transaction.h"
#include "sov/core/json.h"
#include "sov/core/uuid.h"
#include "sov/world/scene_document.h"

namespace sov::cmd {

enum class TransactionKind { Edit, Undo, Redo };
std::string_view ToString(TransactionKind kind);

// One committed transaction. Persisted in the scene journal; also the unit of undo/redo.
struct TransactionRecord {
  Uuid commit_id;                     // the commit token returned to callers
  TransactionKind kind = TransactionKind::Edit;
  std::string actor;
  std::string request_id;             // canonical UUID text, or empty for editor-internal edits
  std::string idempotency_key;        // empty when the request carried none (editor GUI edits)
  std::string request_hash;           // CommandBatch::ContentHash(), empty when no key
  std::uint64_t base_revision = 0;
  std::uint64_t new_revision = 0;
  std::vector<Operation> forward;     // operations applied by this transaction
  std::vector<Operation> inverse;     // operations that undo it
  std::optional<Uuid> target_commit;  // undo/redo: the edit transaction being undone/redone
  std::string label;
  std::int64_t timestamp_ms = 0;      // wall clock, ms since Unix epoch (idempotency retention)
  Json result;                        // the CommandResult JSON originally returned
};

Json RecordToJson(const TransactionRecord& r);
Result<TransactionRecord> RecordFromJson(const Json& j);

enum class ResultStatus {
  Committed,  // applied; revision incremented once
  DryRun,     // validated; nothing changed
  NoChange,   // valid but produced no content change; revision unchanged, nothing recorded
  Rejected,   // validation/conflict failure; nothing changed
};
std::string_view ToString(ResultStatus status);

struct CommandResult {
  std::string request_id;
  ResultStatus status = ResultStatus::Rejected;
  TransactionKind kind = TransactionKind::Edit;
  std::string actor;
  Uuid scene_id;
  std::uint64_t revision_before = 0;
  std::uint64_t revision_after = 0;
  std::vector<OpOutcome> operations;
  std::vector<Error> errors;
  std::vector<std::string> warnings;
  Json diff;                           // scene::DiffToJson for committed/dry-run results
  std::optional<Uuid> commit_id;
  std::optional<Uuid> target_commit;   // undo/redo
  std::string request_hash;
  Json conflict;                       // stale revision: commits newer than base_revision
  bool replayed = false;               // duplicate delivery: `recorded` holds the original result
  Json recorded;

  Json ToJson() const;
  bool ok() const { return status != ResultStatus::Rejected; }
};

struct SessionOptions {
  std::function<std::int64_t()> now_ms;                       // default: system clock
  std::int64_t idempotency_retention_ms = 7LL * 24 * 3600 * 1000;
  std::function<Uuid()> new_id;                               // default: GenerateUuid
};

// In-memory authoring session over one scene document. All mutation paths (CLI batches, editor
// GUI, future agents) go through this class; nothing else edits the document.
// Single owner thread; no internal locking.
class SceneSession {
 public:
  SceneSession(scene::SceneDocument document, std::vector<TransactionRecord> history, SessionOptions options = {});

  const scene::SceneDocument& document() const { return doc_; }
  std::uint64_t revision() const { return doc_.revision; }

  // Full batch semantics, in this order:
  //   1. idempotency: same (actor, key) + same content within retention -> recorded result (replayed);
  //      different content -> idempotency_conflict; older than retention -> idempotency_expired
  //   2. scene_id must match (scene_mismatch)
  //   3. base_revision must equal the current revision (stale_revision, with conflict details)
  //   4. staged whole-batch validation (any failure -> rejected, no change)
  //   5. dry_run -> diff only; otherwise commit, revision + 1
  // Dry runs never consult or reserve the ledger beyond reporting a warning for a known key.
  CommandResult ApplyBatch(const CommandBatch& batch);

  // Editor/internal edit path: same staging, revision and history rules, no idempotency key.
  CommandResult ApplyEdit(const std::string& actor, std::uint64_t base_revision, std::vector<Operation> ops,
                          std::string label);

  // History service. Undo/redo require the current revision as base, produce a new revision,
  // and never rewind the revision counter.
  CommandResult Undo(const std::string& actor, std::uint64_t base_revision);
  CommandResult Redo(const std::string& actor, std::uint64_t base_revision);
  bool CanUndo() const { return !undo_stack_.empty(); }
  bool CanRedo() const { return !redo_stack_.empty(); }
  std::optional<Uuid> NextUndo() const;
  std::optional<Uuid> NextRedo() const;

  const std::vector<TransactionRecord>& history() const { return history_; }
  const TransactionRecord* FindRecord(const Uuid& commit_id) const;
  std::size_t persisted_count() const { return persisted_count_; }
  std::vector<TransactionRecord> PendingRecords() const;
  void MarkAllPersisted() { persisted_count_ = history_.size(); }

 private:
  CommandResult Commit(TransactionKind kind, const std::string& actor, const std::string& request_id,
                       std::uint64_t base_revision, std::vector<Operation> ops, std::string label,
                       const std::optional<Uuid>& target, const CommandBatch* batch, bool dry_run);
  CommandResult StaleRevision(CommandResult r, std::uint64_t base_revision) const;
  void RebuildStacks();
  const TransactionRecord* FindByKey(const std::string& actor, const std::string& key) const;

  scene::SceneDocument doc_;
  std::vector<TransactionRecord> history_;
  std::size_t persisted_count_ = 0;
  std::vector<Uuid> undo_stack_;  // edit commit ids, top = back
  std::vector<Uuid> redo_stack_;
  SessionOptions options_;
};

}  // namespace sov::cmd
