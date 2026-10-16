#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "sov/commands/scene_session.h"
#include "sov/core/fs.h"
#include "sov/world/scene_document.h"

// Scene persistence with a commit journal.
//
// Files next to <scene>.scene.json:
//   <scene>.scene.json.lock     writer lock (flock/LockFileEx); holder description inside
//   <scene>.scene.json.journal  JSON Lines commit journal (epoch / prepare / committed / aborted)
//   <scene>.scene.json.bak      previous committed scene bytes (single-scene backup)
//
// Commit protocol (one scene file; see docs/architecture/command_protocol.md §Durability):
//   1. append+fsync  {"type":"prepare", seq, revision_before, revision_after, scene_sha256_after, records}
//   2. atomic write  <scene>.bak  <- current scene bytes
//   3. atomic write  <scene>      <- new scene bytes (temp + fsync + rename + dir fsync)
//   4. verify        re-read the scene and compare SHA-256
//   5. append+fsync  {"type":"committed", seq}
// Recovery on open: a trailing prepare without a marker is rolled forward if the scene file has
// revision_after and scene_sha256_after, aborted if it still has revision_before, otherwise the
// journal is declared inconsistent and history/idempotency records are not trusted.
namespace sov::cmd {

enum class OpenMode { ReadOnly, Writer };

struct JournalStatus {
  bool present = false;
  bool consistent = true;     // journal's last committed state matches the scene file
  std::vector<std::string> notes;  // recovery actions and warnings
};

// Test-only crash simulation: Commit stops (returns IoFailure "simulated crash") at this point
// without writing anything further, leaving files as a real crash would.
enum class CrashPoint { None, AfterPrepare, AfterReplace };

class SceneStore {
 public:
  // Writer mode acquires the writer lock first (LockUnavailable if held). Read-only mode takes no
  // lock and never modifies any file (recovery decisions are computed in memory only).
  static Result<std::unique_ptr<SceneStore>> Open(const std::filesystem::path& scene_path, OpenMode mode,
                                                  std::string_view tool_name);

  // Creates a new scene file. Fails if the file already exists. Acquires the writer lock.
  static Result<std::unique_ptr<SceneStore>> Create(const std::filesystem::path& scene_path,
                                                    const scene::SceneDocument& doc, std::string_view tool_name);

  ~SceneStore();

  const std::filesystem::path& path() const { return path_; }
  OpenMode mode() const { return mode_; }
  const scene::SceneDocument& document() const { return document_; }
  const std::vector<TransactionRecord>& history() const { return history_; }
  const JournalStatus& journal_status() const { return journal_; }
  const std::vector<std::string>& load_warnings() const { return load_warnings_; }
  const std::string& file_sha256() const { return file_sha256_; }

  // Builds a session over the loaded document and trusted history.
  SceneSession MakeSession(SessionOptions options = {}) const;

  // Persists `doc` and the transaction records that produced it (Writer mode only).
  // Writing an unchanged document with no records is a no-op (no rewrite, no revision change).
  Result<void> Commit(const scene::SceneDocument& doc, const std::vector<TransactionRecord>& records);

  static std::filesystem::path LockPath(const std::filesystem::path& scene);
  static std::filesystem::path JournalPath(const std::filesystem::path& scene);
  static std::filesystem::path BackupPath(const std::filesystem::path& scene);

  static void SetCrashPointForTesting(CrashPoint point);

 private:
  SceneStore() = default;
  Result<void> LoadJournal();
  Result<void> StartEpoch(const std::string& reason);

  std::filesystem::path path_;
  OpenMode mode_ = OpenMode::ReadOnly;
  std::unique_ptr<fs::FileLock> lock_;
  scene::SceneDocument document_;
  std::string file_bytes_;
  std::string file_sha256_;
  std::vector<std::string> load_warnings_;
  std::vector<TransactionRecord> history_;
  JournalStatus journal_;
  std::uint64_t next_seq_ = 1;
  bool need_epoch_ = true;
};

}  // namespace sov::cmd
