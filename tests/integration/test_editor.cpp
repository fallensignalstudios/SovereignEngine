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
  // The TDD §2.4 first-success sequence, through the editor's authoring services, on the packet's
  // First Light (revision 0).
  const auto path = StageScene(ctx.TempDir());
  const std::string original_bytes = Read(path);
  Uuid column, platform, cap;
  scene::TransformComponent original_column;
  const Vec3 glow{0.9, 0.3, 0.05};
  {
    auto opened = EditorDocument::Open(path, "editor:test");
    SOV_REQUIRE_OK(opened);
    EditorDocument& doc = **opened;
    const auto* c = FindByName(doc.document(), "Column 2");
    const auto* p = FindByName(doc.document(), "Raised platform");
    SOV_REQUIRE(c && p);
    column = c->id;
    platform = p->id;
    original_column = c->transform;

    // A cap parented to the column (create + reparent through transform.set, both from the editor).
    SOV_REQUIRE(doc.CreatePrimitive(scene::PrimitiveShape::Box, {0, 0, 0}, "Column 2 Cap").status ==
                cmd::ResultStatus::Committed);
    cap = *doc.selection();
    scene::TransformComponent cap_t = doc.document().Find(cap)->transform;
    cap_t.parent = column;
    cap_t.position = {0.0, 0.55, 0.0};
    SOV_REQUIRE(doc.SetTransform(cap, cap_t).status == cmd::ResultStatus::Committed);

    doc.Select(column);
    SOV_CHECK(doc.selection() == column);
    scene::TransformComponent moved = original_column;
    moved.position.x += 1.25;
    SOV_REQUIRE(doc.SetTransform(column, moved).status == cmd::ResultStatus::Committed);
    SOV_CHECK_EQ(doc.document().Find(column)->transform.position.x, original_column.position.x + 1.25);
    // The child follows its parent in the authoring world (parent-before-child propagation).
    const auto wm = doc.authoring_world().GetWorldMatrix(doc.authoring_world().Find(cap));
    SOV_REQUIRE(wm.has_value());
    SOV_CHECK(NearlyEqual(GetTranslation(*wm).x, original_column.position.x + 1.25, 1e-9));

    SOV_REQUIRE(doc.Undo().status == cmd::ResultStatus::Committed);
    SOV_CHECK(doc.document().Find(column)->transform == original_column);

    const auto* emissive = scene::FindField(scene::MaterialDescriptor(), "emissive_rgb");
    SOV_REQUIRE(doc.SetMaterialField(platform, *emissive, glow).status == cmd::ResultStatus::Committed);
    SOV_CHECK(doc.dirty());
    SOV_CHECK_EQ(doc.revision(), std::uint64_t{5});  // create, reparent, move, undo, material
    SOV_REQUIRE_OK(doc.Save());
    SOV_CHECK(!doc.dirty());
  }
  SOV_CHECK(Read(path) != original_bytes);
  // Reopen in a fresh document: edits persisted, history survived, undo still works.
  auto reopened = EditorDocument::Open(path, "editor:test");
  SOV_REQUIRE_OK(reopened);
  EditorDocument& doc = **reopened;
  SOV_CHECK_EQ(doc.revision(), std::uint64_t{5});
  SOV_CHECK(doc.document().Find(platform)->material->emissive_rgb == glow);
  SOV_CHECK(doc.document().Find(column)->transform == original_column);
  SOV_CHECK(doc.document().Find(cap)->transform.parent == column);
  SOV_CHECK_EQ(doc.session().history().size(), std::size_t{5});
  SOV_REQUIRE(doc.Undo().status == cmd::ResultStatus::Committed);  // undoes the material change
  SOV_CHECK(doc.document().Find(platform)->material->emissive_rgb == Vec3(0, 0, 0));
}

SOV_TEST(editor, unsaved_edits_are_not_written) {
  const auto path = StageScene(ctx.TempDir());
  const std::string before = Read(path);
  {
    auto doc = EditorDocument::Open(path);
    SOV_REQUIRE_OK(doc);
    const auto* platform = FindByName((*doc)->document(), "Raised platform");
    SOV_REQUIRE((*doc)->Rename(platform->id, "Renamed platform").ok());
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
  // Deleting an entity that still has children is refused, as in the CLI (no cascading delete).
  // Copy the id: every committed edit replaces the document, so entity pointers do not survive it.
  const auto* pivot_entity = FindByName(doc.document(), "Column 5");
  SOV_REQUIRE(pivot_entity);
  const Uuid pivot = pivot_entity->id;
  SOV_REQUIRE(doc.CreatePrimitive(scene::PrimitiveShape::Box, {0, 0, 0}, "Column 5 Cap").ok());
  scene::TransformComponent child_t = doc.document().Find(*doc.selection())->transform;
  child_t.parent = pivot;
  SOV_REQUIRE(doc.SetTransform(*doc.selection(), child_t).ok());
  const auto refused = doc.Delete(pivot);
  SOV_CHECK(refused.status == cmd::ResultStatus::Rejected);
  SOV_CHECK_EQ(refused.errors[0].code, ErrorCode::MissingDependency);
}

#if !defined(_WIN32)
SOV_TEST(editor, gui_edit_and_cli_edit_produce_identical_authored_state) {
  // Path A: the editor's authoring services (what the inspector widgets call), then Save.
  const auto a = StageScene(ctx.TempDir() / "gui");
  scene::TransformComponent target;
  scene::MaterialComponent platform_material;
  Uuid column, platform;
  const Vec4 bronze{0.6, 0.35, 0.15, 1.0};
  {
    auto opened = EditorDocument::Open(a, "editor:matthew");
    SOV_REQUIRE_OK(opened);
    EditorDocument& doc = **opened;
    const auto* c = FindByName(doc.document(), "Column 2");
    const auto* p = FindByName(doc.document(), "Raised platform");
    SOV_REQUIRE(c && p);
    column = c->id;
    platform = p->id;
    target = c->transform;
    target.position = {-6.0, 3.5, 2.5};
    target.rotation = QuatFromEulerDegrees({0, 15, 0});
    SOV_REQUIRE(doc.SetTransform(column, target).status == cmd::ResultStatus::Committed);
    SOV_REQUIRE(doc.SetMaterialField(platform, *scene::FindField(scene::MaterialDescriptor(), "base_color_rgba"), bronze)
                    .status == cmd::ResultStatus::Committed);
    platform_material = *doc.document().Find(platform)->material;
    SOV_REQUIRE_OK(doc.Save());
  }

  // Path B: the same two edits as two command batches through the real sovereign-cli executable.
  const auto b = StageScene(ctx.TempDir() / "cli");
  const auto write_batch = [&](const char* file, std::uint64_t base, cmd::Operation op, const char* key) {
    cmd::CommandBatch batch;
    batch.request_id = GenerateUuid();
    batch.actor = "agent:equivalence";
    batch.scene_id = *Uuid::Parse("10000000-0000-4000-8000-000000000001");
    batch.base_revision = base;
    batch.idempotency_key = key;
    batch.dry_run = false;
    batch.operations = {std::move(op)};
    const auto p = ctx.TempDir() / file;
    if (!fs::AtomicWriteFile(p, CanonicalJson(cmd::CommandBatchToJson(batch)))) throw std::runtime_error("write");
    return p;
  };
  const auto b1 = write_batch("move.json", 0, cmd::Operation::SetTransform(column, target), "eq-move");
  const auto b2 = write_batch("color.json", 1, cmd::Operation::SetMaterial(platform, platform_material), "eq-color");
  SOV_CHECK(platform_material.base_color_rgba == bronze);
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
  SOV_CHECK(r.status == cmd::ResultStatus::DryRun);  // the packet example is a dry run
  SOV_CHECK_EQ(r.ToJson()["diff"]["entities_created"].size(), std::size_t{1});
  SOV_CHECK_EQ(r.ToJson()["diff"]["entities_modified"].size(), std::size_t{1});
  SOV_CHECK_EQ((*opened)->revision(), std::uint64_t{0});
  SOV_CHECK(!(*opened)->dirty());
}

SOV_TEST(editor, edits_disabled_while_playing) {
  const auto path = StageScene(ctx.TempDir());
  auto opened = EditorDocument::Open(path);
  SOV_REQUIRE_OK(opened);
  EditorDocument& doc = **opened;
  SOV_REQUIRE_OK(doc.BeginPlay(games::MakeSandboxGame()));
  const Uuid platform = FindByName(doc.document(), "Raised platform")->id;
  const auto r = doc.Rename(platform, "During Play");
  SOV_CHECK(r.status == cmd::ResultStatus::Rejected);
  SOV_CHECK_EQ(r.errors[0].code, ErrorCode::InvalidState);
  SOV_CHECK_EQ(doc.Undo().errors[0].code, ErrorCode::InvalidState);
  doc.EndPlay();
  SOV_CHECK(doc.Rename(platform, "After Play").ok());
}
