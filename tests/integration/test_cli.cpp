// Drives the real sovereign-cli executable as a subprocess (POSIX shell).
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>

#include "sov/commands/scene_store.h"
#include "sov/core/fs.h"
#include "sov/core/json.h"
#include "sov/core/sha256.h"
#include "sov_test.h"

#ifndef SOV_CLI_PATH
#error "SOV_CLI_PATH must be defined"
#endif

using namespace sov;

namespace {

struct Run {
  int exit_code = -1;
  std::string out;
  Json json;
};

#if !defined(_WIN32)
#include <sys/wait.h>
Run Cli(const std::string& args, const std::filesystem::path& cwd) {
  const std::string cmd = std::format("cd '{}' && '{}' {} 2>/dev/null", cwd.string(), SOV_CLI_PATH, args);
  Run r;
  FILE* p = popen(cmd.c_str(), "r");
  if (!p) return r;
  char buf[4096];
  std::size_t n;
  while ((n = fread(buf, 1, sizeof(buf), p)) > 0) r.out.append(buf, n);
  const int status = pclose(p);
  r.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  auto j = ParseStrictJson(r.out);
  if (j) r.json = *j;
  return r;
}
#endif

std::string Read(const std::filesystem::path& p) { return *fs::ReadFile(p, 1 << 26); }

// Copies the checked-in First Light scene and example batch into a fresh working directory.
std::filesystem::path StageFirstLight(sov::test::Context& ctx) {
  const auto dir = ctx.TempDir();
  std::filesystem::copy_file(ctx.SourceDir() / "content/authoring/first_light.scene.json", dir / "first_light.scene.json");
  std::filesystem::copy_file(ctx.SourceDir() / "examples/place_banner.command_batch.json", dir / "place_banner.json");
  return dir;
}

// The example as an explicitly committed request with its own request id and key.
void WriteCommittedVariant(const std::filesystem::path& dir, const char* out_name, const char* key,
                           double emissive_strength = 3.2) {
  Json b = *ParseStrictJson(Read(dir / "place_banner.json"));
  b["dry_run"] = false;
  b["request_id"] = "0b1e0000-0000-4000-8000-00000000c001";
  b["idempotency_key"] = key;
  b["operations"][2]["material"]["emissive_strength"] = emissive_strength;
  if (!fs::AtomicWriteFile(dir / out_name, CanonicalJson(b))) throw std::runtime_error("write failed");
}

}  // namespace

#if !defined(_WIN32)

SOV_TEST(cli, version_reports_build_identity) {
  const Run r = Cli("--version", ctx.TempDir());
  SOV_CHECK_EQ(r.exit_code, 0);
  SOV_CHECK(r.out.find("sovereign-cli ") == 0);
  SOV_CHECK(r.out.find("dependency-lock: sha256:") != std::string::npos);
  SOV_CHECK(r.out.find("never initializes a graphics device") != std::string::npos);
}

SOV_TEST(cli, usage_errors_exit_2) {
  SOV_CHECK_EQ(Cli("frobnicate", ctx.TempDir()).exit_code, 2);
  SOV_CHECK_EQ(Cli("scene teleport --scene x", ctx.TempDir()).exit_code, 2);
}

SOV_TEST(cli, new_validate_inspect) {
  const auto dir = ctx.TempDir();
  const Run created = Cli("scene new --scene hall.scene.json --name \"Test Hall\"", dir);
  SOV_CHECK_EQ(created.exit_code, 0);
  SOV_CHECK(created.json.value("ok", false));
  SOV_CHECK_EQ(Cli("scene new --scene hall.scene.json --name Again", dir).exit_code, 1);  // never overwrites
  const Run valid = Cli("scene validate --scene hall.scene.json --json", dir);
  SOV_CHECK_EQ(valid.exit_code, 0);
  SOV_CHECK(valid.json.value("ok", false));
  SOV_CHECK(valid.json.value("canonical", false));
  SOV_CHECK_EQ(valid.json["entity_count"].get<int>(), 1);
  const Run inspect = Cli("scene inspect --scene hall.scene.json", dir);
  SOV_CHECK_EQ(inspect.exit_code, 0);
  SOV_CHECK(inspect.out.find("Main Camera") != std::string::npos);
  // A corrupted document is reported, not repaired.
  SOV_REQUIRE_OK(fs::AtomicWriteFile(dir / "bad.scene.json", R"({"format":"sovereign.scene","schema_version":9})"));
  const Run bad = Cli("scene validate --scene bad.scene.json --json", dir);
  SOV_CHECK_EQ(bad.exit_code, 1);
  SOV_CHECK_EQ(bad.json["errors"][0]["code"].get<std::string>(), std::string("unknown_schema_version"));
}

SOV_TEST(cli, example_batch_is_a_dry_run_and_changes_nothing) {
  const auto dir = StageFirstLight(ctx);
  const std::string before = Read(dir / "first_light.scene.json");
  const Run r = Cli("scene apply --scene first_light.scene.json --commands place_banner.json --result dry.json", dir);
  SOV_CHECK_EQ(r.exit_code, 0);
  SOV_CHECK_EQ(r.json["status"].get<std::string>(), std::string("dry_run"));
  SOV_CHECK_EQ(r.json["revision_after"].get<int>(), 1);
  SOV_CHECK_EQ(r.json["diff"]["entities_created"].size(), std::size_t{2});
  SOV_CHECK_EQ(r.json["diff"]["entities_modified"].size(), std::size_t{1});
  SOV_CHECK_EQ(Read(dir / "first_light.scene.json"), before);
  // Read-only path: no journal, lock or backup files were created by the dry run.
  SOV_CHECK(!std::filesystem::exists(cmd::SceneStore::JournalPath(dir / "first_light.scene.json")));
  SOV_CHECK(!std::filesystem::exists(cmd::SceneStore::LockPath(dir / "first_light.scene.json")));
  SOV_CHECK(std::filesystem::exists(dir / "dry.json"));
}

SOV_TEST(cli, committed_request_then_retry_then_changed_content) {
  const auto dir = StageFirstLight(ctx);
  WriteCommittedVariant(dir, "commit.json", "place-banner-commit-0001");
  const Run first = Cli("scene apply --scene first_light.scene.json --commands commit.json", dir);
  SOV_CHECK_EQ(first.exit_code, 0);
  SOV_CHECK_EQ(first.json["status"].get<std::string>(), std::string("committed"));
  SOV_CHECK_EQ(first.json["revision_after"].get<int>(), 2);
  const std::string after = Read(dir / "first_light.scene.json");

  const Run retry = Cli("scene apply --scene first_light.scene.json --commands commit.json", dir);
  SOV_CHECK_EQ(retry.exit_code, 0);
  SOV_CHECK_EQ(retry.json["delivery"].get<std::string>(), std::string("replayed"));
  SOV_CHECK_EQ(retry.json["commit_id"], first.json["commit_id"]);
  SOV_CHECK_EQ(Read(dir / "first_light.scene.json"), after);

  WriteCommittedVariant(dir, "changed.json", "place-banner-commit-0001", 9.0);
  const Run changed = Cli("scene apply --scene first_light.scene.json --commands changed.json", dir);
  SOV_CHECK_EQ(changed.exit_code, 1);
  SOV_CHECK_EQ(changed.json["errors"][0]["code"].get<std::string>(), std::string("idempotency_conflict"));
  SOV_CHECK_EQ(Read(dir / "first_light.scene.json"), after);
}

SOV_TEST(cli, stale_revision_is_rejected_with_conflict) {
  const auto dir = StageFirstLight(ctx);
  WriteCommittedVariant(dir, "commit.json", "k-1");
  SOV_REQUIRE(Cli("scene apply --scene first_light.scene.json --commands commit.json", dir).exit_code == 0);
  WriteCommittedVariant(dir, "stale.json", "k-2");  // still claims base_revision 1
  const Run r = Cli("scene apply --scene first_light.scene.json --commands stale.json", dir);
  SOV_CHECK_EQ(r.exit_code, 1);
  SOV_CHECK_EQ(r.json["errors"][0]["code"].get<std::string>(), std::string("stale_revision"));
  SOV_CHECK_EQ(r.json["conflict"]["current_revision"].get<int>(), 2);
}

SOV_TEST(cli, offline_write_refused_while_another_writer_holds_the_lock) {
  const auto dir = StageFirstLight(ctx);
  WriteCommittedVariant(dir, "commit.json", "k-lock");
  const std::string before = Read(dir / "first_light.scene.json");
  {
    // Same lock the editor takes for the lifetime of an open scene.
    auto editor = cmd::SceneStore::Open(dir / "first_light.scene.json", cmd::OpenMode::Writer, "sovereign-editor");
    SOV_REQUIRE_OK(editor);
    const Run refused = Cli("scene apply --scene first_light.scene.json --commands commit.json", dir);
    SOV_CHECK_EQ(refused.exit_code, 3);
    SOV_CHECK_EQ(refused.json["error"]["code"].get<std::string>(), std::string("lock_unavailable"));
    SOV_CHECK(refused.json["error"]["message"].get<std::string>().find("sovereign-editor") != std::string::npos);
    // Dry runs remain available to readers.
    const Run dry = Cli("scene apply --scene first_light.scene.json --commands place_banner.json", dir);
    SOV_CHECK_EQ(dry.exit_code, 0);
    SOV_CHECK_EQ(Cli("scene undo --scene first_light.scene.json --base-revision 1", dir).exit_code, 3);
  }
  SOV_CHECK_EQ(Read(dir / "first_light.scene.json"), before);
  SOV_CHECK_EQ(Cli("scene apply --scene first_light.scene.json --commands commit.json", dir).exit_code, 0);
}

SOV_TEST(cli, undo_redo_and_history) {
  const auto dir = StageFirstLight(ctx);
  const std::string original = Read(dir / "first_light.scene.json");
  WriteCommittedVariant(dir, "commit.json", "k-undo");
  SOV_REQUIRE(Cli("scene apply --scene first_light.scene.json --commands commit.json", dir).exit_code == 0);
  const Run undo = Cli("scene undo --scene first_light.scene.json --base-revision 2", dir);
  SOV_CHECK_EQ(undo.exit_code, 0);
  SOV_CHECK_EQ(undo.json["kind"].get<std::string>(), std::string("undo"));
  SOV_CHECK_EQ(undo.json["revision_after"].get<int>(), 3);
  // Content equals the original except for the revision counter.
  auto now = scene::ParseSceneText(Read(dir / "first_light.scene.json"));
  auto orig = scene::ParseSceneText(original);
  SOV_REQUIRE_OK(now);
  SOV_REQUIRE_OK(orig);
  SOV_CHECK_EQ(scene::ContentHash(*now), scene::ContentHash(*orig));
  SOV_CHECK_EQ(now->revision, std::uint64_t{3});
  SOV_CHECK_EQ(Cli("scene undo --scene first_light.scene.json --base-revision 2", dir).exit_code, 1);  // stale
  const Run redo = Cli("scene redo --scene first_light.scene.json --base-revision 3", dir);
  SOV_CHECK_EQ(redo.exit_code, 0);
  SOV_CHECK_EQ(redo.json["revision_after"].get<int>(), 4);
  const Run history = Cli("scene history --scene first_light.scene.json --json", dir);
  SOV_CHECK_EQ(history.exit_code, 0);
  // The journal began at revision 1 (the checked-in scene), so it holds edit, undo and redo.
  SOV_CHECK_EQ(history.json["transactions"].size(), std::size_t{3});
}

SOV_TEST(cli, invalid_batch_rejected_scene_untouched) {
  const auto dir = StageFirstLight(ctx);
  const std::string before = Read(dir / "first_light.scene.json");
  Json b = *ParseStrictJson(Read(dir / "place_banner.json"));
  b["dry_run"] = false;
  b["idempotency_key"] = "bad-1";
  b["operations"][1]["entity_id"] = b["operations"][0]["entity_id"];  // duplicate create in one batch
  SOV_REQUIRE_OK(fs::AtomicWriteFile(dir / "bad.json", CanonicalJson(b)));
  const Run r = Cli("scene apply --scene first_light.scene.json --commands bad.json", dir);
  SOV_CHECK_EQ(r.exit_code, 1);
  SOV_CHECK_EQ(r.json["status"].get<std::string>(), std::string("rejected"));
  SOV_CHECK_EQ(r.json["errors"][0]["code"].get<std::string>(), std::string("duplicate_entity"));
  SOV_CHECK_EQ(r.json["errors"][0]["op_index"].get<int>(), 1);
  SOV_CHECK_EQ(Read(dir / "first_light.scene.json"), before);
  SOV_REQUIRE_OK(fs::AtomicWriteFile(dir / "garbage.json", "{ not json"));
  SOV_CHECK_EQ(Cli("scene apply --scene first_light.scene.json --commands garbage.json", dir).exit_code, 1);
}

SOV_TEST(cli, project_root_confines_paths) {
  const auto dir = StageFirstLight(ctx);
  std::filesystem::create_directories(dir / "project");
  std::filesystem::copy_file(dir / "first_light.scene.json", dir / "project/first_light.scene.json");
  SOV_CHECK_EQ(Cli("scene validate --project-root project --scene first_light.scene.json", dir).exit_code, 0);
  const Run escape = Cli("scene validate --project-root project --scene ../first_light.scene.json --json", dir);
  SOV_CHECK_EQ(escape.exit_code, 1);
  SOV_CHECK_EQ(escape.json["error"]["code"].get<std::string>(), std::string("permission_denied"));
  SOV_CHECK_EQ(Cli("scene validate --project-root project --scene /etc/passwd", dir).exit_code, 1);
  std::filesystem::create_symlink(dir / "first_light.scene.json", dir / "project/outside.scene.json");
  SOV_CHECK_EQ(Cli("scene validate --project-root project --scene outside.scene.json", dir).exit_code, 1);
}

#endif
