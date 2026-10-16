#include "sov/commands/scene_store.h"

#include <format>
#include <map>

#include "sov/core/build_info.h"
#include "sov/core/log.h"
#include "sov/core/sha256.h"

namespace sov::cmd {
namespace {

CrashPoint g_crash_point = CrashPoint::None;
constexpr std::size_t kMaxJournalBytes = 256u * 1024u * 1024u;

std::filesystem::path WithSuffix(const std::filesystem::path& p, std::string_view suffix) {
  return std::filesystem::path(p.string() + std::string(suffix));
}

JsonLimits JournalLineLimits() {
  JsonLimits l;
  l.max_bytes = 64u * 1024u * 1024u;
  l.max_depth = 32;
  l.max_string_bytes = 64u * 1024u;
  return l;
}

}  // namespace

void SceneStore::SetCrashPointForTesting(CrashPoint point) { g_crash_point = point; }

std::filesystem::path SceneStore::LockPath(const std::filesystem::path& s) { return WithSuffix(s, ".lock"); }
std::filesystem::path SceneStore::JournalPath(const std::filesystem::path& s) { return WithSuffix(s, ".journal"); }
std::filesystem::path SceneStore::BackupPath(const std::filesystem::path& s) { return WithSuffix(s, ".bak"); }

SceneStore::~SceneStore() = default;

Result<std::unique_ptr<SceneStore>> SceneStore::Open(const std::filesystem::path& scene_path, OpenMode mode,
                                                     std::string_view tool_name) {
  std::unique_ptr<SceneStore> store(new SceneStore());
  store->path_ = scene_path;
  store->mode_ = mode;
  std::error_code link_ec;
  if (mode == OpenMode::Writer && std::filesystem::is_symlink(scene_path, link_ec)) {
    // An atomic rename would replace the link itself rather than the file it points to.
    return MakeError(ErrorCode::PermissionDenied,
                     std::format("{} is a symbolic link; writers require the real scene file path", scene_path.string()));
  }
  if (mode == OpenMode::Writer) {
    auto lock = fs::FileLock::TryAcquire(LockPath(scene_path), fs::ProcessDescription(tool_name));
    if (!lock) return lock.take_error();
    store->lock_ = std::move(lock).value();
  }
  auto loaded = scene::LoadSceneFile(scene_path);
  if (!loaded) return loaded.take_error();
  store->document_ = std::move(loaded->document);
  store->file_bytes_ = std::move(loaded->file_bytes);
  store->file_sha256_ = std::move(loaded->file_sha256);
  store->load_warnings_ = std::move(loaded->warnings);
  if (auto r = store->LoadJournal(); !r) return r.take_error();
  if (store->lock_ && !store->lock_->reclaimed_from().empty()) {
    store->journal_.notes.push_back(std::format(
        "reclaimed a stale writer lock left by \"{}\" (that writer ended without releasing it); journal recovery "
        "above decided any commit it left unfinished",
        store->lock_->reclaimed_from()));
  }
  return store;
}

Result<std::unique_ptr<SceneStore>> SceneStore::Create(const std::filesystem::path& scene_path,
                                                       const scene::SceneDocument& doc, std::string_view tool_name) {
  scene::Diagnostics d;
  scene::ValidateStructure(doc, d);
  if (!d.ok()) return d.First();
  std::unique_ptr<SceneStore> store(new SceneStore());
  store->path_ = scene_path;
  store->mode_ = OpenMode::Writer;
  auto lock = fs::FileLock::TryAcquire(LockPath(scene_path), fs::ProcessDescription(tool_name));
  if (!lock) return lock.take_error();
  store->lock_ = std::move(lock).value();
  if (std::filesystem::exists(scene_path)) {
    return MakeError(ErrorCode::InvalidState, std::format("{} already exists; refusing to overwrite", scene_path.string()));
  }
  const std::string bytes = scene::SerializeScene(doc);
  if (auto r = fs::AtomicWriteFile(scene_path, bytes); !r) return r.take_error();
  store->document_ = doc;
  store->file_bytes_ = bytes;
  store->file_sha256_ = Sha256Hex(bytes);
  if (auto r = store->StartEpoch("scene created"); !r) return r.take_error();
  return store;
}

Result<void> SceneStore::StartEpoch(const std::string& reason) {
  const auto journal = JournalPath(path_);
  if (std::filesystem::exists(journal)) {
    std::string stamp = CurrentUtcTimestamp();
    for (char& c : stamp) {
      if (c == ':') c = '-';
    }
    const auto orphan = WithSuffix(journal, ".orphaned-" + stamp);
    std::error_code ec;
    std::filesystem::rename(journal, orphan, ec);
    if (ec) return MakeError(ErrorCode::IoFailure, std::format("cannot set aside old journal: {}", ec.message()));
    journal_.notes.push_back(std::format("previous journal preserved as {}", orphan.filename().string()));
  }
  Json epoch = Json::object();
  epoch["type"] = "epoch";
  epoch["journal_format"] = kJournalFormatVersion;
  epoch["scene_id"] = document_.scene_id.ToString();
  epoch["started_at_revision"] = document_.revision;
  epoch["scene_sha256"] = file_sha256_;
  epoch["created_at"] = CurrentUtcTimestamp();
  epoch["reason"] = reason;
  if (auto r = fs::AtomicWriteFile(journal, CompactJson(epoch) + "\n"); !r) return r;
  history_.clear();
  next_seq_ = 1;
  need_epoch_ = false;
  journal_.present = true;
  journal_.consistent = true;
  return Ok();
}

Result<void> SceneStore::LoadJournal() {
  const auto journal_path = JournalPath(path_);
  journal_ = {};
  history_.clear();
  if (!std::filesystem::exists(journal_path)) {
    need_epoch_ = true;
    return Ok();
  }
  journal_.present = true;
  auto bytes = fs::ReadFile(journal_path, kMaxJournalBytes);
  if (!bytes) return bytes.take_error();

  // Split into lines; a final line without '\n' is a torn append.
  std::vector<std::string> lines;
  std::size_t start = 0, good_end = 0;
  bool torn = false;
  while (start < bytes->size()) {
    const std::size_t nl = bytes->find('\n', start);
    if (nl == std::string::npos) {
      torn = true;
      break;
    }
    lines.push_back(bytes->substr(start, nl - start));
    start = nl + 1;
    good_end = start;
  }

  const auto inconsistent = [&](std::string why) -> Result<void> {
    journal_.consistent = false;
    journal_.notes.push_back(why + "; history and idempotency records from this journal are not trusted");
    history_.clear();
    need_epoch_ = true;
    return Ok();
  };

  if (torn) {
    journal_.notes.push_back("ignored a torn (incomplete) final journal line");
    if (mode_ == OpenMode::Writer) {
      if (auto r = fs::AtomicWriteFile(journal_path, bytes->substr(0, good_end)); !r) return r;
    }
  }
  if (lines.empty()) return inconsistent("journal is empty");

  struct Prepare {
    std::uint64_t seq;
    std::uint64_t revision_before, revision_after;
    std::string sha_before, sha_after;
    std::vector<TransactionRecord> records;
  };
  std::optional<Prepare> pending;
  std::uint64_t current_revision = 0;
  std::string current_sha;
  std::uint64_t max_seq = 0;
  std::vector<TransactionRecord> committed;

  try {
  for (std::size_t i = 0; i < lines.size(); ++i) {
    auto j = ParseStrictJson(lines[i], JournalLineLimits());
    if (!j || !j->is_object() || !j->contains("type")) {
      return inconsistent(std::format("journal line {} is malformed", i + 1));
    }
    const std::string type = (*j)["type"].get<std::string>();
    if (i == 0) {
      if (type != "epoch") return inconsistent("journal does not start with an epoch record");
      if ((*j).value("journal_format", 0) != kJournalFormatVersion) {
        return inconsistent("journal format version is not supported");
      }
      if ((*j).value("scene_id", std::string()) != document_.scene_id.ToString()) {
        return inconsistent("journal belongs to a different scene_id");
      }
      current_revision = (*j)["started_at_revision"].get<std::uint64_t>();
      current_sha = (*j)["scene_sha256"].get<std::string>();
      continue;
    }
    if (type == "prepare") {
      if (pending) return inconsistent(std::format("journal line {}: prepare follows an unresolved prepare", i + 1));
      Prepare p;
      p.seq = (*j)["seq"].get<std::uint64_t>();
      p.revision_before = (*j)["revision_before"].get<std::uint64_t>();
      p.revision_after = (*j)["revision_after"].get<std::uint64_t>();
      p.sha_before = (*j)["scene_sha256_before"].get<std::string>();
      p.sha_after = (*j)["scene_sha256_after"].get<std::string>();
      for (const auto& rj : (*j)["records"]) {
        auto rec = RecordFromJson(rj);
        if (!rec) return inconsistent(std::format("journal line {}: {}", i + 1, rec.error().message));
        p.records.push_back(std::move(rec).value());
      }
      if (p.revision_before != current_revision || p.sha_before != current_sha) {
        return inconsistent(std::format("journal line {}: prepare does not follow the previous committed state", i + 1));
      }
      max_seq = std::max(max_seq, p.seq);
      pending = std::move(p);
    } else if (type == "committed" || type == "aborted") {
      const auto seq = (*j)["seq"].get<std::uint64_t>();
      if (!pending || pending->seq != seq) {
        return inconsistent(std::format("journal line {}: {} marker without a matching prepare", i + 1, type));
      }
      if (type == "committed") {
        current_revision = pending->revision_after;
        current_sha = pending->sha_after;
        for (auto& r : pending->records) committed.push_back(std::move(r));
      }
      pending.reset();
    } else {
      return inconsistent(std::format("journal line {}: unknown record type \"{}\"", i + 1, type));
    }
  }
  } catch (const nlohmann::json::exception& e) {
    return inconsistent(std::format("journal record has unexpected structure ({})", e.what()));
  }

  if (pending) {
    const auto mark = [&](std::string_view type, std::string_view reason) -> Result<void> {
      if (mode_ != OpenMode::Writer) return Ok();
      Json m = Json{{"type", type}, {"seq", pending->seq}, {"reason", reason}, {"time", CurrentUtcTimestamp()}};
      return fs::AppendDurable(journal_path, CompactJson(m) + "\n");
    };
    if (document_.revision == pending->revision_after && file_sha256_ == pending->sha_after) {
      if (auto r = mark("committed", "recovery: scene file matches the prepared commit"); !r) return r;
      journal_.notes.push_back(std::format(
          "recovery: commit seq {} (revision {} -> {}) reached the scene file but was not marked; confirmed as committed{}",
          pending->seq, pending->revision_before, pending->revision_after,
          mode_ == OpenMode::Writer ? "" : " (read-only: marker not written)"));
      current_revision = pending->revision_after;
      current_sha = pending->sha_after;
      for (auto& r : pending->records) committed.push_back(std::move(r));
    } else if (document_.revision == pending->revision_before && file_sha256_ == pending->sha_before) {
      if (auto r = mark("aborted", "recovery: scene file still holds the pre-commit state"); !r) return r;
      journal_.notes.push_back(std::format(
          "recovery: commit seq {} (revision {} -> {}) never reached the scene file; discarded{}", pending->seq,
          pending->revision_before, pending->revision_after,
          mode_ == OpenMode::Writer ? "" : " (read-only: marker not written)"));
    } else {
      return inconsistent(std::format("unresolved commit seq {} matches neither the pre- nor post-commit scene state",
                                      pending->seq));
    }
  }

  if (current_revision != document_.revision || current_sha != file_sha256_) {
    return inconsistent(std::format(
        "scene file (revision {}, sha256 {}) does not match the journal's last committed state (revision {}, sha256 {}); "
        "it was changed outside the engine's commit path",
        document_.revision, file_sha256_.substr(0, 12), current_revision, current_sha.substr(0, 12)));
  }

  history_ = std::move(committed);
  next_seq_ = max_seq + 1;
  need_epoch_ = false;
  return Ok();
}

SceneSession SceneStore::MakeSession(SessionOptions options) const {
  return SceneSession(document_, history_, std::move(options));
}

Result<void> SceneStore::Commit(const scene::SceneDocument& doc, const std::vector<TransactionRecord>& records) {
  if (mode_ != OpenMode::Writer) {
    return MakeError(ErrorCode::PermissionDenied, "scene was opened read-only; writing requires the writer lock");
  }
  if (doc.scene_id != document_.scene_id) {
    return MakeError(ErrorCode::SceneMismatch, "document scene_id differs from the opened scene");
  }
  const std::string bytes = scene::SerializeScene(doc);
  if (records.empty() && bytes == file_bytes_) return Ok();  // unchanged: no rewrite
  if (doc.revision < document_.revision) {
    return MakeError(ErrorCode::InvalidState, std::format("refusing to write revision {} over revision {}", doc.revision,
                                                          document_.revision));
  }
  if (!records.empty() &&
      (records.front().base_revision != document_.revision || records.back().new_revision != doc.revision)) {
    return MakeError(ErrorCode::InvalidState, "transaction records do not chain from the stored revision to the new one");
  }
  if (records.empty() && doc.revision == document_.revision && bytes != file_bytes_) {
    return MakeError(ErrorCode::InvalidState, "content changed without a revision change or transaction record");
  }

  if (need_epoch_) {
    if (auto r = StartEpoch(journal_.consistent ? "first commit" : "journal inconsistent with scene file"); !r) return r;
  }

  const auto journal_path = JournalPath(path_);
  const std::string sha_after = Sha256Hex(bytes);
  const std::uint64_t seq = next_seq_;
  Json prepare = Json::object();
  prepare["type"] = "prepare";
  prepare["seq"] = seq;
  prepare["revision_before"] = document_.revision;
  prepare["revision_after"] = doc.revision;
  prepare["scene_sha256_before"] = file_sha256_;
  prepare["scene_sha256_after"] = sha_after;
  prepare["time"] = CurrentUtcTimestamp();
  Json recs = Json::array();
  for (const auto& r : records) recs.push_back(RecordToJson(r));
  prepare["records"] = std::move(recs);
  if (auto r = fs::AppendDurable(journal_path, CompactJson(prepare) + "\n"); !r) return r;
  next_seq_ = seq + 1;

  const auto abort = [&](Error e) -> Result<void> {
    Json m = Json{{"type", "aborted"}, {"seq", seq}, {"reason", e.message}, {"time", CurrentUtcTimestamp()}};
    if (auto r = fs::AppendDurable(journal_path, CompactJson(m) + "\n"); !r) {
      LogWarn("store", "could not record aborted commit seq {}: {}", seq, r.error().message);
    }
    return e;
  };

  if (g_crash_point == CrashPoint::AfterPrepare) {
    g_crash_point = CrashPoint::None;
    return MakeError(ErrorCode::IoFailure, "simulated crash after journal prepare (test)");
  }
  if (auto r = fs::AtomicWriteFile(BackupPath(path_), file_bytes_); !r) return abort(r.take_error());
  if (auto r = fs::AtomicWriteFile(path_, bytes); !r) return abort(r.take_error());
  if (g_crash_point == CrashPoint::AfterReplace) {
    g_crash_point = CrashPoint::None;
    return MakeError(ErrorCode::IoFailure, "simulated crash after scene replace (test)");
  }
  auto verify = fs::ReadFile(path_, scene::SceneLimits::kMaxFileBytes);
  if (!verify || Sha256Hex(*verify) != sha_after) {
    return MakeError(ErrorCode::IoFailure, "verification read-back of the saved scene did not match what was written");
  }
  Json marker = Json{{"type", "committed"}, {"seq", seq}, {"time", CurrentUtcTimestamp()}};
  if (auto r = fs::AppendDurable(journal_path, CompactJson(marker) + "\n"); !r) {
    LogWarn("store", "scene saved, but the commit marker could not be written ({}); recovery will confirm it on next open",
            r.error().message);
  }

  document_ = doc;
  file_bytes_ = bytes;
  file_sha256_ = sha_after;
  for (const auto& r : records) history_.push_back(r);
  return Ok();
}

}  // namespace sov::cmd
