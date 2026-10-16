#include <fstream>

#include "scene_fixtures.h"
#include "sov/commands/scene_store.h"
#include "sov/core/fs.h"
#include "sov/core/sha256.h"
#include "sov_test.h"

using namespace sov;
using namespace sov::cmd;

namespace {

std::filesystem::path NewScene(sov::test::Context& ctx, const char* name = "small.scene.json") {
  const auto path = ctx.TempDir() / name;
  auto store = SceneStore::Create(path, fixtures::SmallScene(), "test");
  if (!store) throw std::runtime_error(store.error().Describe());
  return path;
}

CommandBatch MoveBatch(std::uint64_t base, const Vec3& to, std::string key) {
  CommandBatch b;
  b.request_id = *Uuid::Parse("5c0e0000-0000-4000-8000-00000000f001");
  b.actor = "cli:test";
  b.scene_id = fixtures::kSceneId;
  b.base_revision = base;
  b.idempotency_key = std::move(key);
  b.dry_run = false;
  scene::TransformComponent t;
  t.position = to;
  b.operations = {Operation::SetTransform(fixtures::kParentId, t)};
  return b;
}

// Opens the scene as a writer, applies the batch and commits, like `sovereign-cli scene apply`.
CommandResult ApplyAndCommit(const std::filesystem::path& path, const CommandBatch& b, std::string* error = nullptr) {
  auto store = SceneStore::Open(path, OpenMode::Writer, "test-cli");
  if (!store) {
    if (error) *error = store.error().Describe();
    return {};
  }
  SceneSession s = (*store)->MakeSession();
  CommandResult r = s.ApplyBatch(b);
  if (r.status == ResultStatus::Committed) {
    auto c = (*store)->Commit(s.document(), s.PendingRecords());
    if (!c && error) *error = c.error().Describe();
  }
  return r;
}

std::size_t LineCount(const std::filesystem::path& p) {
  std::ifstream in(p);
  std::size_t n = 0;
  std::string line;
  while (std::getline(in, line)) ++n;
  return n;
}

std::string Bytes(const std::filesystem::path& p) { return *fs::ReadFile(p, 1 << 26); }

}  // namespace

SOV_TEST(store, create_commit_reopen_preserves_document_and_history) {
  const auto path = NewScene(ctx);
  std::string err;
  const auto r = ApplyAndCommit(path, MoveBatch(3, {4, 5, 6}, "move-1"), &err);
  SOV_REQUIRE_MSG(r.status == ResultStatus::Committed && err.empty(), err);
  auto reopened = SceneStore::Open(path, OpenMode::ReadOnly, "test");
  SOV_REQUIRE_OK(reopened);
  SOV_CHECK_EQ((*reopened)->document().revision, std::uint64_t{4});
  SOV_CHECK((*reopened)->document().Find(fixtures::kParentId)->transform.position == Vec3(4, 5, 6));
  SOV_CHECK_EQ((*reopened)->history().size(), std::size_t{1});
  SOV_CHECK((*reopened)->journal_status().consistent);
  // Backup holds the previous committed bytes.
  auto backup = scene::ParseSceneText(Bytes(SceneStore::BackupPath(path)));
  SOV_REQUIRE_OK(backup);
  SOV_CHECK_EQ(backup->revision, std::uint64_t{3});
}

SOV_TEST(store, retry_after_restart_replays_recorded_result) {
  const auto path = NewScene(ctx);
  const CommandBatch b = MoveBatch(3, {1, 1, 1}, "restart-key");
  const auto first = ApplyAndCommit(path, b);
  SOV_REQUIRE(first.status == ResultStatus::Committed);
  const std::string bytes_after_first = Bytes(path);
  // New process (fresh store + session): the ledger comes back from the journal.
  const auto retry = ApplyAndCommit(path, b);
  SOV_CHECK(retry.replayed);
  SOV_CHECK_EQ(retry.ToJson()["commit_id"], first.ToJson()["commit_id"]);
  SOV_CHECK_EQ(Bytes(path), bytes_after_first);
  // Same key, different content after restart: conflict.
  const auto changed = ApplyAndCommit(path, MoveBatch(3, {2, 2, 2}, "restart-key"));
  SOV_CHECK_EQ(changed.errors[0].code, ErrorCode::IdempotencyConflict);
}

SOV_TEST(store, writer_lock_refuses_second_writer_but_allows_readers) {
  const auto path = NewScene(ctx);
  auto editor = SceneStore::Open(path, OpenMode::Writer, "sovereign-editor");
  SOV_REQUIRE_OK(editor);
  auto cli = SceneStore::Open(path, OpenMode::Writer, "sovereign-cli");
  SOV_REQUIRE(!cli.ok());
  SOV_CHECK_EQ(cli.error().code, ErrorCode::LockUnavailable);
  SOV_CHECK(cli.error().message.find("sovereign-editor") != std::string::npos);
  auto reader = SceneStore::Open(path, OpenMode::ReadOnly, "sovereign-cli");
  SOV_CHECK(reader.ok());
  // A read-only store refuses to write.
  auto denied = (*reader)->Commit((*reader)->document(), {});
  SOV_REQUIRE(!denied.ok());
  SOV_CHECK_EQ(denied.error().code, ErrorCode::PermissionDenied);
  editor.value().reset();
  SOV_CHECK(SceneStore::Open(path, OpenMode::Writer, "sovereign-cli").ok());
}

SOV_TEST(store, crash_after_prepare_recovers_as_aborted) {
  const auto path = NewScene(ctx);
  const std::string original = Bytes(path);
  const CommandBatch b = MoveBatch(3, {8, 8, 8}, "crash-1");
  SceneStore::SetCrashPointForTesting(CrashPoint::AfterPrepare);
  std::string err;
  ApplyAndCommit(path, b, &err);
  SOV_CHECK(err.find("simulated crash") != std::string::npos);
  SOV_CHECK_EQ(Bytes(path), original);  // scene untouched

  auto store = SceneStore::Open(path, OpenMode::Writer, "test");
  SOV_REQUIRE_OK(store);
  SOV_CHECK((*store)->journal_status().consistent);
  SOV_REQUIRE(!(*store)->journal_status().notes.empty());
  SOV_CHECK((*store)->journal_status().notes[0].find("discarded") != std::string::npos);
  SOV_CHECK((*store)->history().empty());
  store.value().reset();
  // The request never committed, so a retry applies it once.
  const auto retry = ApplyAndCommit(path, b, &err);
  SOV_CHECK(retry.status == ResultStatus::Committed);
  SOV_CHECK(!retry.replayed);
}

SOV_TEST(store, crash_after_replace_recovers_as_committed) {
  const auto path = NewScene(ctx);
  const CommandBatch b = MoveBatch(3, {9, 9, 9}, "crash-2");
  SceneStore::SetCrashPointForTesting(CrashPoint::AfterReplace);
  std::string err;
  const auto first = ApplyAndCommit(path, b, &err);
  SOV_CHECK(err.find("simulated crash") != std::string::npos);

  auto store = SceneStore::Open(path, OpenMode::Writer, "test");
  SOV_REQUIRE_OK(store);
  SOV_CHECK((*store)->journal_status().consistent);
  SOV_REQUIRE(!(*store)->journal_status().notes.empty());
  SOV_CHECK((*store)->journal_status().notes[0].find("confirmed as committed") != std::string::npos);
  SOV_CHECK_EQ((*store)->history().size(), std::size_t{1});
  store.value().reset();
  // The commit is durable and recorded: the retry replays instead of applying twice.
  const auto retry = ApplyAndCommit(path, b, &err);
  SOV_CHECK(retry.replayed);
  SOV_CHECK_EQ(retry.ToJson()["commit_id"], first.ToJson()["commit_id"]);
}

SOV_TEST(store, failed_atomic_write_preserves_last_valid_scene) {
  const auto path = NewScene(ctx);
  const std::string original = Bytes(path);
  // Fail the scene replace (the backup write succeeds first).
  fs::SetFailPointForTesting(fs::FailPoint::AtomicWriteBeforeRename, /*trigger_after_n_calls=*/1);
  std::string err;
  const auto r = ApplyAndCommit(path, MoveBatch(3, {1, 2, 3}, "io-1"), &err);
  SOV_CHECK(err.find("injected failure") != std::string::npos);
  SOV_CHECK_EQ(Bytes(path), original);
  auto store = SceneStore::Open(path, OpenMode::ReadOnly, "test");
  SOV_REQUIRE_OK(store);
  SOV_CHECK((*store)->journal_status().consistent);  // prepare was explicitly marked aborted
  SOV_CHECK((*store)->history().empty());
  (void)r;
}

SOV_TEST(store, external_edit_invalidates_history_and_starts_new_epoch) {
  const auto path = NewScene(ctx);
  SOV_REQUIRE(ApplyAndCommit(path, MoveBatch(3, {1, 0, 0}, "e-1")).status == ResultStatus::Committed);
  // Someone edits the file outside the engine (still a valid document).
  auto doc = scene::ParseSceneText(Bytes(path));
  SOV_REQUIRE_OK(doc);
  doc->Find(fixtures::kParentId)->name = "Hand Edited";
  SOV_REQUIRE_OK(fs::AtomicWriteFile(path, scene::SerializeScene(*doc)));

  auto store = SceneStore::Open(path, OpenMode::Writer, "test");
  SOV_REQUIRE_OK(store);
  SOV_CHECK(!(*store)->journal_status().consistent);
  SOV_CHECK((*store)->history().empty());  // old idempotency records are not trusted
  SceneSession s = (*store)->MakeSession();
  const auto r = s.ApplyBatch(MoveBatch(4, {2, 0, 0}, "e-1"));  // same key: not replayed from the stale journal
  SOV_REQUIRE(r.status == ResultStatus::Committed);
  SOV_REQUIRE_OK((*store)->Commit(s.document(), s.PendingRecords()));
  bool orphan_found = false;
  for (const auto& e : std::filesystem::directory_iterator(ctx.TempDir())) {
    orphan_found = orphan_found || e.path().filename().string().find(".journal.orphaned-") != std::string::npos;
  }
  SOV_CHECK(orphan_found);
  store.value().reset();
  auto again = SceneStore::Open(path, OpenMode::ReadOnly, "test");
  SOV_REQUIRE_OK(again);
  SOV_CHECK((*again)->journal_status().consistent);
  SOV_CHECK_EQ((*again)->history().size(), std::size_t{1});
}

SOV_TEST(store, unchanged_save_does_not_rewrite) {
  const auto path = NewScene(ctx);
  const auto mtime = std::filesystem::last_write_time(path);
  const auto journal_lines = LineCount(SceneStore::JournalPath(path));
  auto store = SceneStore::Open(path, OpenMode::Writer, "test");
  SOV_REQUIRE_OK(store);
  SOV_REQUIRE_OK((*store)->Commit((*store)->document(), {}));
  SOV_CHECK(std::filesystem::last_write_time(path) == mtime);
  SOV_CHECK_EQ(LineCount(SceneStore::JournalPath(path)), journal_lines);
  SOV_CHECK_EQ((*store)->document().revision, std::uint64_t{3});
}

SOV_TEST(store, torn_journal_tail_is_ignored) {
  const auto path = NewScene(ctx);
  SOV_REQUIRE(ApplyAndCommit(path, MoveBatch(3, {1, 0, 0}, "t-1")).status == ResultStatus::Committed);
  SOV_REQUIRE_OK(fs::AppendDurable(SceneStore::JournalPath(path), R"({"type":"prepare","seq":9,"revisi)"));
  auto store = SceneStore::Open(path, OpenMode::Writer, "test");
  SOV_REQUIRE_OK(store);
  SOV_CHECK((*store)->journal_status().consistent);
  SOV_CHECK_EQ((*store)->history().size(), std::size_t{1});
  SceneSession s = (*store)->MakeSession();
  SOV_REQUIRE(s.ApplyBatch(MoveBatch(4, {2, 0, 0}, "t-2")).status == ResultStatus::Committed);
  SOV_REQUIRE_OK((*store)->Commit(s.document(), s.PendingRecords()));
  store.value().reset();
  auto again = SceneStore::Open(path, OpenMode::ReadOnly, "test");
  SOV_REQUIRE_OK(again);
  SOV_CHECK((*again)->journal_status().consistent);
  SOV_CHECK_EQ((*again)->history().size(), std::size_t{2});
}

SOV_TEST(store, fifty_open_save_close_cycles) {
  const auto path = NewScene(ctx);
  for (int i = 0; i < 50; ++i) {
    auto store = SceneStore::Open(path, OpenMode::Writer, "cycle");
    SOV_REQUIRE_OK(store);
    SceneSession s = (*store)->MakeSession();
    const auto r = s.ApplyEdit("cycle", s.revision(),
                               {Operation::Rename(fixtures::kParentId, "Cycle " + std::to_string(i))}, "cycle rename");
    SOV_REQUIRE(r.status == ResultStatus::Committed);
    SOV_REQUIRE_OK((*store)->Commit(s.document(), s.PendingRecords()));
  }
  auto final_store = SceneStore::Open(path, OpenMode::ReadOnly, "check");
  SOV_REQUIRE_OK(final_store);
  SOV_CHECK_EQ((*final_store)->document().revision, std::uint64_t{53});
  SOV_CHECK_EQ((*final_store)->document().Find(fixtures::kParentId)->name, std::string("Cycle 49"));
  SOV_CHECK_EQ((*final_store)->history().size(), std::size_t{50});
  SOV_CHECK((*final_store)->journal_status().consistent);
  // Exactly the expected files: scene, lock, journal, backup. No leaked temporaries.
  std::size_t files = 0;
  for (const auto& e : std::filesystem::directory_iterator(ctx.TempDir())) {
    (void)e;
    ++files;
  }
  SOV_CHECK_EQ(files, std::size_t{4});
  SOV_CHECK_EQ(LineCount(SceneStore::JournalPath(path)), std::size_t{1 + 2 * 50});
}

SOV_TEST(store, writer_refuses_symlinked_scene_path) {
  const auto path = NewScene(ctx);
  const auto link = ctx.TempDir() / "link.scene.json";
  std::filesystem::create_symlink(path, link);
  auto w = SceneStore::Open(link, OpenMode::Writer, "test");
  SOV_REQUIRE(!w.ok());
  SOV_CHECK_EQ(w.error().code, ErrorCode::PermissionDenied);
  SOV_CHECK(SceneStore::Open(link, OpenMode::ReadOnly, "test").ok());
}
