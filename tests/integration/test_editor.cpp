// Editor authoring services (the layer the GUI panels call), exercised headless.
#include <cstdio>
#include <format>

#include "sov/commands/scene_store.h"
#include "sov/core/fs.h"
#include "sov/core/sha256.h"
#include "sov/editor/editor_document.h"
#include "sov/games/sandbox.h"
#include "sov/world/reflection.h"
#include "sov_test.h"

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

using namespace sov;
using namespace sov::editor;

namespace {


std::string Read(const std::filesystem::path& p) { return *fs::ReadFile(p, 1 << 26); }

std::filesystem::path StageScene(const std::filesystem::path& dir) {
  std::filesystem::create_directories(dir);
  const auto dst = dir / "first_light.scene.json";
  std::filesystem::copy_file(sov::test::Context::SourceDir() / "content/authoring/first_light.scene.json", dst,
                             std::filesystem::copy_options::overwrite_existing);
  return dst;
}

const scene::EntityData* FindByName(const scene::SceneDocument& d, const std::string& name) {
  for (const auto& e : d.entities) {
    if (e.name == name) return &e;
  }
  return nullptr;
}

#if !defined(_WIN32)
int RunCli(const std::string& args) {
  const std::string cmd = std::format("'{}' {} > /dev/null 2>&1", SOV_CLI_PATH, args);
  const int status = std::system(cmd.c_str());
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
#endif

}  // namespace

SOV_TEST(editor, open_holds_writer_lock_until_closed) {
  const auto path = StageScene(ctx.TempDir());
  {
    auto doc = EditorDocument::Open(path);
    SOV_REQUIRE_OK(doc);
    auto other = cmd::SceneStore::Open(path, cmd::OpenMode::Writer, "sovereign-cli");
    SOV_REQUIRE(!other.ok());
    SOV_CHECK_EQ(other.error().code, ErrorCode::LockUnavailable);
  }
  SOV_CHECK(cmd::SceneStore::Open(path, cmd::OpenMode::Writer, "sovereign-cli").ok());
}

SOV_TEST(editor, move_column_undo_material_save_close_reopen) {
  // The TDD §2.4 first-success sequence, through the editor's authoring services.
  const auto path = StageScene(ctx.TempDir());
  const std::string original_bytes = Read(path);
  Uuid column, altar;
  scene::TransformComponent original_column;
  {
    auto opened = EditorDocument::Open(path, "editor:test");
    SOV_REQUIRE_OK(opened);
    EditorDocument& doc = **opened;
    const auto* c = FindByName(doc.document(), "Column W3");
    const auto* a = FindByName(doc.document(), "Altar");
    SOV_REQUIRE(c && a);
    column = c->id;
    altar = a->id;
    original_column = c->transform;

    doc.Select(column);
    SOV_CHECK(doc.selection() == column);
    scene::TransformComponent moved = original_column;
    moved.position.x += 1.25;
    SOV_REQUIRE(doc.SetTransform(column, moved, false).status == cmd::ResultStatus::Committed);
    SOV_CHECK_EQ(doc.document().Find(column)->transform.position.x, original_column.position.x + 1.25);
    // The column's children follow it in the authoring world (hierarchy).
    const auto* shaft = FindByName(doc.document(), "Column W3 Shaft");
    SOV_REQUIRE(shaft);
    const auto wm = doc.authoring_world().GetWorldMatrix(doc.authoring_world().Find(shaft->id));
    SOV_REQUIRE(wm.has_value());
    SOV_CHECK(NearlyEqual(GetTranslation(*wm).x, original_column.position.x + 1.25, 1e-9));

    SOV_REQUIRE(doc.Undo().status == cmd::ResultStatus::Committed);
    SOV_CHECK(doc.document().Find(column)->transform == original_column);

    const auto* rough = scene::FindField(scene::MaterialDescriptor(), "roughness");
    SOV_REQUIRE(doc.SetMaterialField(altar, *rough, 0.12).status == cmd::ResultStatus::Committed);
    SOV_CHECK(doc.dirty());
    SOV_CHECK_EQ(doc.revision(), std::uint64_t{4});  // move, undo, material: three new revisions
    SOV_REQUIRE_OK(doc.Save());
    SOV_CHECK(!doc.dirty());
  }
  SOV_CHECK(Read(path) != original_bytes);
  // Reopen in a fresh document: edits persisted, history survived, undo still works.
  auto reopened = EditorDocument::Open(path, "editor:test");
  SOV_REQUIRE_OK(reopened);
  EditorDocument& doc = **reopened;
  SOV_CHECK_EQ(doc.revision(), std::uint64_t{4});
  SOV_CHECK_EQ(doc.document().Find(altar)->material->roughness, 0.12);
  SOV_CHECK(doc.document().Find(column)->transform == original_column);
  SOV_CHECK_EQ(doc.session().history().size(), std::size_t{3});
  SOV_REQUIRE(doc.Undo().status == cmd::ResultStatus::Committed);  // undoes the material change
  SOV_CHECK(doc.document().Find(altar)->material->roughness != 0.12);
}

SOV_TEST(editor, unsaved_edits_are_not_written) {
  const auto path = StageScene(ctx.TempDir());
  const std::string before = Read(path);
  {
    auto doc = EditorDocument::Open(path);
    SOV_REQUIRE_OK(doc);
    const auto* altar = FindByName((*doc)->document(), "Altar");
    SOV_REQUIRE((*doc)->Rename(altar->id, "Renamed Altar").ok());
    SOV_CHECK((*doc)->dirty());
  }
  SOV_CHECK_EQ(Read(path), before);
}

SOV_TEST(editor, selection_is_by_uuid_and_cleared_when_entity_deleted) {
  const auto path = StageScene(ctx.TempDir());
  auto opened = EditorDocument::Open(path);
  SOV_REQUIRE_OK(opened);
  EditorDocument& doc = **opened;
  auto created = doc.CreatePrimitive(scene::PrimitiveShape::Cylinder, {1, 2, 3}, "New Pillar");
  SOV_REQUIRE(created.status == cmd::ResultStatus::Committed);
  const Uuid id = *doc.selection();  // creation selects the new entity
  const auto gen = doc.authoring_world_generation();
  SOV_CHECK(doc.authoring_world().IsValid(doc.authoring_world().Find(id)));
  SOV_REQUIRE(doc.Rename(id, "Renamed Pillar").ok());
  SOV_CHECK(doc.authoring_world_generation() > gen);  // world rebuilt; selection kept by UUID
  SOV_CHECK(doc.selection() == id);
  SOV_REQUIRE(doc.Delete(id).status == cmd::ResultStatus::Committed);
  SOV_CHECK(!doc.selection().has_value());
  // Deleting a column pivot with children is refused, as in the CLI.
  const auto* pivot = FindByName(doc.document(), "Column E2");
  SOV_REQUIRE(pivot);
  const auto refused = doc.Delete(pivot->id);
  SOV_CHECK(refused.status == cmd::ResultStatus::Rejected);
  SOV_CHECK_EQ(refused.errors[0].code, ErrorCode::MissingDependency);
}

#if !defined(_WIN32)
SOV_TEST(editor, gui_edit_and_cli_edit_produce_identical_authored_state) {
  // Path A: the editor's authoring services (what the inspector widgets call), then Save.
  const auto a = StageScene(ctx.TempDir() / "gui");
  scene::TransformComponent target;
  Uuid column, altar;
  {
    auto opened = EditorDocument::Open(a, "editor:matthew");
    SOV_REQUIRE_OK(opened);
    EditorDocument& doc = **opened;
    const auto* c = FindByName(doc.document(), "Column W3");
    const auto* al = FindByName(doc.document(), "Altar");
    SOV_REQUIRE(c && al);
    column = c->id;
    altar = al->id;
    target = c->transform;
    target.position = {-6.0, 0.0, 2.5};
    target.rotation = QuatFromEulerDegrees({0, 15, 0});
    SOV_REQUIRE(doc.SetTransform(column, target, false).status == cmd::ResultStatus::Committed);
    SOV_REQUIRE(doc.SetMaterialField(altar, *scene::FindField(scene::MaterialDescriptor(), "base_color"),
                                     Vec3{0.6, 0.35, 0.15})
                    .status == cmd::ResultStatus::Committed);
    SOV_REQUIRE_OK(doc.Save());
  }

  // Path B: the same two edits as two command batches through the real sovereign-cli executable.
  const auto b = StageScene(ctx.TempDir() / "cli");
  const auto write_batch = [&](const char* file, std::uint64_t base, cmd::Operation op, const char* key) {
    cmd::CommandBatch batch;
    batch.request_id = GenerateUuid();
    batch.actor = "agent:equivalence";
    batch.scene_id = *Uuid::Parse("8d4684d6-93cd-5d04-a868-b4d839bafda7");
    batch.base_revision = base;
    batch.idempotency_key = key;
    batch.dry_run = false;
    batch.operations = {std::move(op)};
    const auto p = ctx.TempDir() / file;
    if (!fs::AtomicWriteFile(p, CanonicalJson(cmd::CommandBatchToJson(batch)))) throw std::runtime_error("write");
    return p;
  };
  const auto b1 = write_batch("move.json", 1, cmd::Operation::SetTransform(column, target, false), "eq-move");
  const auto b2 = write_batch("color.json", 2,
                              cmd::Operation::SetMaterial(altar, {{scene::FindField(scene::MaterialDescriptor(), "base_color"),
                                                                   Vec3{0.6, 0.35, 0.15}}}),
                              "eq-color");
  SOV_REQUIRE(RunCli(std::format("scene apply --scene '{}' --commands '{}'", b.string(), b1.string())) == 0);
  SOV_REQUIRE(RunCli(std::format("scene apply --scene '{}' --commands '{}'", b.string(), b2.string())) == 0);

  // Same authored document, byte for byte (revision included: both paths made two commits).
  SOV_CHECK_EQ(Sha256Hex(Read(a)), Sha256Hex(Read(b)));
  SOV_CHECK_EQ(Read(a), Read(b));
}
#endif

SOV_TEST(editor, batch_file_applies_through_same_service) {
  const auto path = StageScene(ctx.TempDir());
  auto opened = EditorDocument::Open(path);
  SOV_REQUIRE_OK(opened);
  const auto r = (*opened)->ApplyBatchFile(ctx.SourceDir() / "examples/place_banner.command_batch.json");
  SOV_CHECK(r.status == cmd::ResultStatus::DryRun);  // the example is a dry run
  SOV_CHECK_EQ((*opened)->revision(), std::uint64_t{1});
  SOV_CHECK(!(*opened)->dirty());
}

SOV_TEST(editor, edits_disabled_while_playing) {
  const auto path = StageScene(ctx.TempDir());
  auto opened = EditorDocument::Open(path);
  SOV_REQUIRE_OK(opened);
  EditorDocument& doc = **opened;
  SOV_REQUIRE_OK(doc.BeginPlay(games::MakeSandboxGame()));
  const auto* altar = FindByName(doc.document(), "Altar");
  const auto r = doc.Rename(altar->id, "During Play");
  SOV_CHECK(r.status == cmd::ResultStatus::Rejected);
  SOV_CHECK_EQ(r.errors[0].code, ErrorCode::InvalidState);
  SOV_CHECK_EQ(doc.Undo().errors[0].code, ErrorCode::InvalidState);
  doc.EndPlay();
  SOV_CHECK(doc.Rename(altar->id, "After Play").ok());
}
