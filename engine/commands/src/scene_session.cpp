#include "sov/commands/scene_session.h"

#include <chrono>
#include <format>
#include <set>

#include "sov/core/build_info.h"
#include "sov/world/scene_diff.h"

namespace sov::cmd {
namespace {

std::int64_t SystemNowMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

Json ErrorToJson(const Error& e) {
  Json j = Json::object();
  j["code"] = ToString(e.code);
  j["message"] = e.message;
  j["path"] = e.path.empty() ? Json(nullptr) : Json(e.path);
  j["op_index"] = e.op_index >= 0 ? Json(e.op_index) : Json(nullptr);
  j["entity_id"] = e.entity_id.empty() ? Json(nullptr) : Json(e.entity_id);
  return j;
}

Json OpsToJson(const std::vector<Operation>& ops) {
  Json a = Json::array();
  for (const auto& op : ops) a.push_back(OperationToJson(op));
  return a;
}

Result<std::vector<Operation>> OpsFromJson(const Json& a, const std::string& path) {
  if (!a.is_array()) return MakeError(ErrorCode::SchemaError, "expected an operation array", path);
  std::vector<Operation> ops;
  for (std::size_t i = 0; i < a.size(); ++i) {
    auto op = ParseOperation(a[i], JsonPointerAppend(path, i));
    if (!op) return op.take_error();
    ops.push_back(std::move(op).value());
  }
  return ops;
}

std::optional<TransactionKind> ParseKind(std::string_view s) {
  if (s == "edit") return TransactionKind::Edit;
  if (s == "undo") return TransactionKind::Undo;
  if (s == "redo") return TransactionKind::Redo;
  return std::nullopt;
}

std::set<std::string> TouchedEntities(const TransactionRecord& r) {
  std::set<std::string> ids;
  for (const auto& op : r.forward) ids.insert(op.entity_id.ToString());
  return ids;
}

}  // namespace

std::string_view ToString(TransactionKind kind) {
  switch (kind) {
    case TransactionKind::Edit: return "edit";
    case TransactionKind::Undo: return "undo";
    case TransactionKind::Redo: return "redo";
  }
  return "edit";
}

std::string_view ToString(ResultStatus status) {
  switch (status) {
    case ResultStatus::Committed: return "committed";
    case ResultStatus::DryRun: return "dry_run";
    case ResultStatus::NoChange: return "no_change";
    case ResultStatus::Rejected: return "rejected";
  }
  return "rejected";
}

Json RecordToJson(const TransactionRecord& r) {
  Json j = Json::object();
  j["commit_id"] = r.commit_id.ToString();
  j["kind"] = ToString(r.kind);
  j["actor"] = r.actor;
  j["request_id"] = r.request_id;
  j["idempotency_key"] = r.idempotency_key;
  j["request_hash"] = r.request_hash;
  j["base_revision"] = r.base_revision;
  j["new_revision"] = r.new_revision;
  j["target_commit"] = r.target_commit ? Json(r.target_commit->ToString()) : Json(nullptr);
  j["label"] = r.label;
  j["timestamp_ms"] = r.timestamp_ms;
  j["forward"] = OpsToJson(r.forward);
  j["inverse"] = OpsToJson(r.inverse);
  j["result"] = r.result;
  return j;
}

Result<TransactionRecord> RecordFromJson(const Json& j) {
  try {
    TransactionRecord r;
    auto id = Uuid::Parse(j.at("commit_id").get<std::string>());
    auto kind = ParseKind(j.at("kind").get<std::string>());
    if (!id || !kind) return MakeError(ErrorCode::SchemaError, "journal record has an invalid commit_id or kind");
    r.commit_id = *id;
    r.kind = *kind;
    r.actor = j.at("actor").get<std::string>();
    r.request_id = j.at("request_id").get<std::string>();
    r.idempotency_key = j.at("idempotency_key").get<std::string>();
    r.request_hash = j.at("request_hash").get<std::string>();
    r.base_revision = j.at("base_revision").get<std::uint64_t>();
    r.new_revision = j.at("new_revision").get<std::uint64_t>();
    if (!j.at("target_commit").is_null()) {
      auto t = Uuid::Parse(j.at("target_commit").get<std::string>());
      if (!t) return MakeError(ErrorCode::SchemaError, "journal record has an invalid target_commit");
      r.target_commit = *t;
    }
    r.label = j.at("label").get<std::string>();
    r.timestamp_ms = j.at("timestamp_ms").get<std::int64_t>();
    auto fwd = OpsFromJson(j.at("forward"), "/forward");
    if (!fwd) return fwd.take_error();
    auto inv = OpsFromJson(j.at("inverse"), "/inverse");
    if (!inv) return inv.take_error();
    r.forward = std::move(fwd).value();
    r.inverse = std::move(inv).value();
    r.result = j.at("result");
    return r;
  } catch (const nlohmann::json::exception& e) {
    return MakeError(ErrorCode::SchemaError, std::format("malformed journal record: {}", e.what()));
  }
}

Json CommandResult::ToJson() const {
  if (replayed) {
    Json j = recorded;
    j["delivery"] = "replayed";
    return j;
  }
  Json j = Json::object();
  j["protocol"] = "sovereign.command_result";
  j["protocol_version"] = kCommandProtocolVersion;
  j["request_id"] = request_id.empty() ? Json(nullptr) : Json(request_id);
  j["status"] = ToString(status);
  j["delivery"] = "original";
  j["kind"] = ToString(kind);
  j["actor"] = actor;
  j["scene_id"] = scene_id.ToString();
  j["revision_before"] = revision_before;
  j["revision_after"] = revision_after;
  j["commit_id"] = commit_id ? Json(commit_id->ToString()) : Json(nullptr);
  j["target_commit"] = target_commit ? Json(target_commit->ToString()) : Json(nullptr);
  j["request_hash"] = request_hash.empty() ? Json(nullptr) : Json(request_hash);
  Json ops = Json::array();
  for (const auto& o : operations) {
    ops.push_back(Json{{"index", o.index}, {"op", ToString(o.kind)}, {"entity_id", o.entity_id.ToString()},
                       {"status", ToString(o.status)}});
  }
  j["operations"] = std::move(ops);
  Json errs = Json::array();
  for (const auto& e : errors) errs.push_back(ErrorToJson(e));
  j["errors"] = std::move(errs);
  j["warnings"] = warnings;
  j["diff"] = diff.is_null() ? Json(nullptr) : diff;
  j["conflict"] = conflict.is_null() ? Json(nullptr) : conflict;
  return j;
}

SceneSession::SceneSession(scene::SceneDocument document, std::vector<TransactionRecord> history, SessionOptions options)
    : doc_(std::move(document)), history_(std::move(history)), options_(std::move(options)) {
  if (!options_.now_ms) options_.now_ms = SystemNowMs;
  if (!options_.new_id) options_.new_id = [] { return GenerateUuid(); };
  persisted_count_ = history_.size();
  RebuildStacks();
}

void SceneSession::RebuildStacks() {
  undo_stack_.clear();
  redo_stack_.clear();
  for (const auto& r : history_) {
    switch (r.kind) {
      case TransactionKind::Edit:
        undo_stack_.push_back(r.commit_id);
        redo_stack_.clear();
        break;
      case TransactionKind::Undo:
        if (!undo_stack_.empty()) undo_stack_.pop_back();
        if (r.target_commit) redo_stack_.push_back(*r.target_commit);
        break;
      case TransactionKind::Redo:
        if (!redo_stack_.empty()) redo_stack_.pop_back();
        if (r.target_commit) undo_stack_.push_back(*r.target_commit);
        break;
    }
  }
}

std::optional<Uuid> SceneSession::NextUndo() const {
  return undo_stack_.empty() ? std::nullopt : std::optional<Uuid>(undo_stack_.back());
}
std::optional<Uuid> SceneSession::NextRedo() const {
  return redo_stack_.empty() ? std::nullopt : std::optional<Uuid>(redo_stack_.back());
}

const TransactionRecord* SceneSession::FindRecord(const Uuid& id) const {
  for (const auto& r : history_) {
    if (r.commit_id == id) return &r;
  }
  return nullptr;
}

const TransactionRecord* SceneSession::FindByKey(const std::string& actor, const std::string& key) const {
  for (auto it = history_.rbegin(); it != history_.rend(); ++it) {
    if (!it->idempotency_key.empty() && it->actor == actor && it->idempotency_key == key) return &*it;
  }
  return nullptr;
}

std::vector<TransactionRecord> SceneSession::PendingRecords() const {
  return {history_.begin() + static_cast<std::ptrdiff_t>(persisted_count_), history_.end()};
}

CommandResult SceneSession::StaleRevision(CommandResult r, std::uint64_t base_revision) const {
  r.status = ResultStatus::Rejected;
  Error e = MakeError(ErrorCode::StaleRevision,
                      std::format("base_revision {} is stale; the scene is at revision {}. Re-read the scene and "
                                  "resubmit against the current revision.",
                                  base_revision, doc_.revision),
                      "/base_revision");
  r.errors.push_back(e);
  Json since = Json::array();
  bool complete = base_revision < doc_.revision;
  std::uint64_t expected = base_revision;
  for (const auto& rec : history_) {
    if (rec.new_revision <= base_revision) continue;
    if (rec.base_revision != expected) complete = false;
    expected = rec.new_revision;
    Json touched = Json::array();
    for (const auto& id : TouchedEntities(rec)) touched.push_back(id);
    since.push_back(Json{{"commit_id", rec.commit_id.ToString()},
                         {"kind", ToString(rec.kind)},
                         {"actor", rec.actor},
                         {"base_revision", rec.base_revision},
                         {"new_revision", rec.new_revision},
                         {"label", rec.label},
                         {"entities", std::move(touched)}});
  }
  if (expected != doc_.revision) complete = false;
  r.conflict = Json{{"base_revision", base_revision},
                    {"current_revision", doc_.revision},
                    {"commits_since_base", std::move(since)},
                    {"history_complete", complete}};
  return r;
}

CommandResult SceneSession::Commit(TransactionKind kind, const std::string& actor, const std::string& request_id,
                                   std::uint64_t base_revision, std::vector<Operation> ops, std::string label,
                                   const std::optional<Uuid>& target, const CommandBatch* batch, bool dry_run) {
  CommandResult r;
  r.request_id = request_id;
  r.kind = kind;
  r.actor = actor;
  r.scene_id = doc_.scene_id;
  r.revision_before = doc_.revision;
  r.revision_after = doc_.revision;
  r.target_commit = target;
  if (batch) r.request_hash = batch->ContentHash();

  if (base_revision != doc_.revision) return StaleRevision(std::move(r), base_revision);

  StageResult staged = StageOperations(doc_, ops);
  r.operations = staged.outcomes;
  if (!staged.ok()) {
    r.status = ResultStatus::Rejected;
    r.errors = staged.errors;
    return r;
  }

  scene::SceneDiff diff = scene::DiffScenes(doc_, staged.staged);
  if (diff.empty() && kind == TransactionKind::Edit) {
    r.status = dry_run ? ResultStatus::DryRun : ResultStatus::NoChange;
    r.warnings.push_back("operations produce no content change; revision unchanged and nothing recorded");
    r.diff = scene::DiffToJson(diff);
    return r;
  }
  if (dry_run) {
    r.status = ResultStatus::DryRun;
    r.diff = scene::DiffToJson(diff);
    r.warnings.push_back(std::format("dry run: nothing was written; committing against revision {} would produce revision {}",
                                     doc_.revision, doc_.revision + 1));
    return r;
  }

  staged.staged.revision = doc_.revision + 1;
  diff.revision_after = staged.staged.revision;

  TransactionRecord rec;
  rec.commit_id = options_.new_id();
  rec.kind = kind;
  rec.actor = actor;
  rec.request_id = request_id;
  if (batch) {
    rec.idempotency_key = batch->idempotency_key;
    rec.request_hash = r.request_hash;
  }
  rec.base_revision = doc_.revision;
  rec.new_revision = staged.staged.revision;
  rec.forward = std::move(ops);
  rec.inverse = std::move(staged.inverse);
  rec.target_commit = target;
  rec.label = std::move(label);
  rec.timestamp_ms = options_.now_ms();

  r.status = ResultStatus::Committed;
  for (auto& o : r.operations) {
    if (o.status == OpStatus::Valid) o.status = OpStatus::Applied;
  }
  r.revision_after = rec.new_revision;
  r.commit_id = rec.commit_id;
  r.diff = scene::DiffToJson(diff);
  rec.result = r.ToJson();

  doc_ = std::move(staged.staged);
  switch (kind) {
    case TransactionKind::Edit:
      undo_stack_.push_back(rec.commit_id);
      redo_stack_.clear();
      break;
    case TransactionKind::Undo:
      undo_stack_.pop_back();
      redo_stack_.push_back(*target);
      break;
    case TransactionKind::Redo:
      redo_stack_.pop_back();
      undo_stack_.push_back(*target);
      break;
  }
  history_.push_back(std::move(rec));
  return r;
}

CommandResult SceneSession::ApplyBatch(const CommandBatch& batch) {
  CommandResult r;
  r.request_id = batch.request_id.ToString();
  r.actor = batch.actor;
  r.scene_id = doc_.scene_id;
  r.revision_before = r.revision_after = doc_.revision;
  r.request_hash = batch.ContentHash();

  if (const TransactionRecord* prior = FindByKey(batch.actor, batch.idempotency_key)) {
    const std::int64_t age = options_.now_ms() - prior->timestamp_ms;
    if (age > options_.idempotency_retention_ms) {
      r.status = ResultStatus::Rejected;
      r.errors.push_back(MakeError(
          ErrorCode::IdempotencyExpired,
          std::format("idempotency key \"{}\" for actor \"{}\" was used by commit {} (revision {}) {} hours ago, beyond the "
                      "{}-hour retention window. Its outcome is unknown to this request; inspect the scene and submit a "
                      "new request with a new key.",
                      batch.idempotency_key, batch.actor, prior->commit_id.ToString(), prior->new_revision,
                      age / 3600000, options_.idempotency_retention_ms / 3600000),
          "/idempotency_key"));
      return r;
    }
    if (prior->request_hash != r.request_hash) {
      r.status = ResultStatus::Rejected;
      r.errors.push_back(MakeError(
          ErrorCode::IdempotencyConflict,
          std::format("idempotency key \"{}\" for actor \"{}\" was already used by commit {} with different request "
                      "content (recorded hash {}, this request {}). Use a new key for a new action.",
                      batch.idempotency_key, batch.actor, prior->commit_id.ToString(), prior->request_hash,
                      r.request_hash),
          "/idempotency_key"));
      return r;
    }
    if (batch.dry_run) {
      r.status = ResultStatus::DryRun;
      r.warnings.push_back(std::format(
          "idempotency key \"{}\" was already committed as {} (revision {}); a non-dry-run delivery would return that "
          "recorded result without applying anything",
          batch.idempotency_key, prior->commit_id.ToString(), prior->new_revision));
      return r;
    }
    r.replayed = true;
    r.recorded = prior->result;
    return r;
  }

  if (batch.scene_id != doc_.scene_id) {
    r.status = ResultStatus::Rejected;
    r.errors.push_back(MakeError(ErrorCode::SceneMismatch,
                                 std::format("batch targets scene {}, but this document is scene {}",
                                             batch.scene_id.ToString(), doc_.scene_id.ToString()),
                                 "/scene_id"));
    return r;
  }

  return Commit(TransactionKind::Edit, batch.actor, batch.request_id.ToString(), batch.base_revision, batch.operations,
                std::format("batch {} ({} operation(s))", batch.request_id.ToString(), batch.operations.size()),
                std::nullopt, &batch, batch.dry_run);
}

CommandResult SceneSession::ApplyEdit(const std::string& actor, std::uint64_t base_revision, std::vector<Operation> ops,
                                      std::string label) {
  return Commit(TransactionKind::Edit, actor, "", base_revision, std::move(ops), std::move(label), std::nullopt, nullptr,
                false);
}

CommandResult SceneSession::Undo(const std::string& actor, std::uint64_t base_revision) {
  if (base_revision == doc_.revision && !CanUndo()) {
    CommandResult r;
    r.kind = TransactionKind::Undo;
    r.actor = actor;
    r.scene_id = doc_.scene_id;
    r.revision_before = r.revision_after = doc_.revision;
    r.status = ResultStatus::Rejected;
    r.errors.push_back(MakeError(ErrorCode::NothingToUndo, "there is no transaction to undo"));
    return r;
  }
  const TransactionRecord* target = CanUndo() ? FindRecord(undo_stack_.back()) : nullptr;
  std::vector<Operation> ops = target ? target->inverse : std::vector<Operation>{};
  const std::string label = target ? "Undo: " + target->label : "Undo";
  const std::optional<Uuid> target_id = target ? std::optional<Uuid>(target->commit_id) : std::nullopt;
  return Commit(TransactionKind::Undo, actor, "", base_revision, std::move(ops), label, target_id, nullptr, false);
}

CommandResult SceneSession::Redo(const std::string& actor, std::uint64_t base_revision) {
  if (base_revision == doc_.revision && !CanRedo()) {
    CommandResult r;
    r.kind = TransactionKind::Redo;
    r.actor = actor;
    r.scene_id = doc_.scene_id;
    r.revision_before = r.revision_after = doc_.revision;
    r.status = ResultStatus::Rejected;
    r.errors.push_back(MakeError(ErrorCode::NothingToRedo, "there is no undone transaction to redo"));
    return r;
  }
  const TransactionRecord* target = CanRedo() ? FindRecord(redo_stack_.back()) : nullptr;
  std::vector<Operation> ops = target ? target->forward : std::vector<Operation>{};
  const std::string label = target ? "Redo: " + target->label : "Redo";
  const std::optional<Uuid> target_id = target ? std::optional<Uuid>(target->commit_id) : std::nullopt;
  return Commit(TransactionKind::Redo, actor, "", base_revision, std::move(ops), label, target_id, nullptr, false);
}

}  // namespace sov::cmd
