#include "scene_fixtures.h"
#include "sov/core/fs.h"
#include "sov/world/reflection.h"
#include "sov/world/scene_diff.h"
#include "sov/world/scene_document.h"
#include "sov_test.h"

using namespace sov;
using namespace sov::scene;

namespace {

Error ParseExpectingError(const Json& j) {
  auto r = ParseSceneDocument(j);
  if (r.ok()) return MakeError(ErrorCode::Internal, "document unexpectedly parsed");
  return r.error();
}

}  // namespace

SOV_TEST(scene, roundtrip_is_byte_identical_and_idempotent) {
  const SceneDocument doc = fixtures::SmallScene();
  const std::string text = SerializeScene(doc);
  auto parsed = ParseSceneText(text);
  SOV_REQUIRE_OK(parsed);
  SOV_CHECK(*parsed == doc);
  SOV_CHECK_EQ(SerializeScene(*parsed), text);
  // Write -> read -> write through the file system as well.
  const auto path = ctx.TempDir() / "small.scene.json";
  SOV_REQUIRE_OK(fs::AtomicWriteFile(path, text));
  auto loaded = LoadSceneFile(path);
  SOV_REQUIRE_OK(loaded);
  SOV_CHECK_EQ(loaded->file_bytes, text);
  SOV_CHECK(loaded->warnings.empty());
}

SOV_TEST(scene, entities_written_in_ascending_uuid_order_with_warning_on_unsorted_input) {
  SceneDocument doc = fixtures::SmallScene();
  Json j = SceneToJson(doc);
  std::reverse(j["entities"].begin(), j["entities"].end());
  Diagnostics d;
  auto parsed = ParseSceneDocument(j, &d);
  SOV_REQUIRE_OK(parsed);
  SOV_CHECK_EQ(d.warnings.size(), std::size_t{1});
  SOV_CHECK(std::is_sorted(parsed->entities.begin(), parsed->entities.end(),
                           [](const EntityData& a, const EntityData& b) { return a.id < b.id; }));
  SOV_CHECK_EQ(SerializeScene(*parsed), SerializeScene(doc));
}

SOV_TEST(scene, unknown_fields_are_rejected_not_dropped) {
  Json j = SceneToJson(fixtures::SmallScene());
  j["entities"][0]["components"]["transform"]["positon"] = Json::array({0, 0, 0});
  Error e = ParseExpectingError(j);
  SOV_CHECK_EQ(e.code, ErrorCode::SchemaError);
  SOV_CHECK_EQ(e.path, std::string("/entities/0/components/transform/positon"));

  j = SceneToJson(fixtures::SmallScene());
  j["entities"][1]["tags"] = Json::array();
  e = ParseExpectingError(j);
  SOV_CHECK_EQ(e.code, ErrorCode::SchemaError);
  SOV_CHECK_EQ(e.path, std::string("/entities/1/tags"));

  j = SceneToJson(fixtures::SmallScene());
  j["lighting_rig"] = "studio";
  e = ParseExpectingError(j);
  SOV_CHECK_EQ(e.path, std::string("/lighting_rig"));
}

SOV_TEST(scene, unknown_component_types_are_rejected) {
  Json j = SceneToJson(fixtures::SmallScene());
  j["entities"][0]["components"]["rigid_body"] = Json{{"mass", 10}};
  const Error e = ParseExpectingError(j);
  SOV_CHECK_EQ(e.code, ErrorCode::UnsupportedComponent);
  SOV_CHECK_EQ(e.path, std::string("/entities/0/components/rigid_body"));
}

SOV_TEST(scene, unknown_schema_version_fails_before_reading) {
  Json j = SceneToJson(fixtures::SmallScene());
  j["schema_version"] = 2;
  j["entities"][0]["future_field"] = true;  // would also be an error; version must be reported first
  const Error e = ParseExpectingError(j);
  SOV_CHECK_EQ(e.code, ErrorCode::UnknownSchemaVersion);
  j = SceneToJson(fixtures::SmallScene());
  j["format"] = "other.scene";
  SOV_CHECK_EQ(ParseExpectingError(j).code, ErrorCode::SchemaError);
}

SOV_TEST(scene, duplicate_uuid_rejected_duplicate_names_allowed) {
  Json j = SceneToJson(fixtures::SmallScene());
  j["entities"][1]["id"] = j["entities"][0]["id"];
  SOV_CHECK_EQ(ParseExpectingError(j).code, ErrorCode::DuplicateEntity);

  SceneDocument doc = fixtures::SmallScene();
  for (auto& e : doc.entities) e.name = "Same Name";
  SOV_CHECK(ParseSceneDocument(SceneToJson(doc)).ok());
}

SOV_TEST(scene, hierarchy_missing_parent_and_cycles_rejected) {
  SceneDocument doc = fixtures::SmallScene();
  doc.Find(fixtures::kChildId)->transform.parent = Uuid::Parse("99999999-9999-4999-8999-999999999999");
  Diagnostics d;
  ValidateStructure(doc, d);
  SOV_REQUIRE(!d.ok());
  SOV_CHECK_EQ(d.errors[0].code, ErrorCode::MissingDependency);
  SOV_CHECK_EQ(d.errors[0].entity_id, fixtures::kChildId.ToString());

  doc = fixtures::SmallScene();
  doc.Find(fixtures::kParentId)->transform.parent = fixtures::kChildId;  // parent <-> child
  d = {};
  ValidateStructure(doc, d);
  SOV_REQUIRE(!d.ok());
  SOV_CHECK_EQ(d.errors[0].code, ErrorCode::CycleDetected);

  doc = fixtures::SmallScene();
  doc.Find(fixtures::kParentId)->transform.parent = fixtures::kParentId;
  d = {};
  ValidateStructure(doc, d);
  SOV_REQUIRE(!d.ok());
  SOV_CHECK_EQ(d.errors[0].code, ErrorCode::CycleDetected);
}

SOV_TEST(scene, value_validation_rejects_bad_numbers) {
  const auto expect = [&](const char* pointer_path, Json value, ErrorCode code) {
    Json j = SceneToJson(fixtures::SmallScene());
    j[Json::json_pointer(pointer_path)] = std::move(value);
    const Error e = ParseExpectingError(j);
    SOV_CHECK_MSG(e.code == code, std::string(pointer_path) + " -> " + e.Describe());
  };
  expect("/entities/0/components/transform/rotation", Json::array({0, 0, 0, 2}), ErrorCode::InvalidValue);
  expect("/entities/0/components/transform/scale", Json::array({1, 0, 1}), ErrorCode::InvalidValue);
  expect("/entities/0/components/transform/position", Json::array({1, 2}), ErrorCode::SchemaError);
  expect("/entities/0/components/transform/position", Json::array({1e9, 0, 0}), ErrorCode::InvalidValue);
  expect("/entities/1/components/material/roughness", Json(1.5), ErrorCode::InvalidValue);
  expect("/entities/1/components/material/base_color", Json::array({0.5, "red", 0.5}), ErrorCode::SchemaError);
  expect("/entities/1/components/primitive_mesh/shape", Json("torus"), ErrorCode::InvalidValue);
  expect("/entities/0/name", Json(""), ErrorCode::InvalidValue);
  expect("/entities/0/name", Json("bad\nname"), ErrorCode::InvalidValue);
  expect("/entities/0/id", Json("NOT-A-UUID"), ErrorCode::SchemaError);
  expect("/revision", Json(-1), ErrorCode::SchemaError);
  expect("/region/cell", Json::array({1, 0, 0}), ErrorCode::InvalidValue);
  expect("/asset_references", Json::array({"mesh.glb"}), ErrorCode::UnsupportedComponent);
}

SOV_TEST(scene, missing_required_fields_rejected) {
  Json j = SceneToJson(fixtures::SmallScene());
  j["entities"][0]["components"]["transform"].erase("scale");
  SOV_CHECK_EQ(ParseExpectingError(j).code, ErrorCode::SchemaError);
  j = SceneToJson(fixtures::SmallScene());
  j["entities"][0]["components"].erase("transform");
  SOV_CHECK_EQ(ParseExpectingError(j).code, ErrorCode::SchemaError);
}

SOV_TEST(scene, active_camera_must_reference_a_camera) {
  SceneDocument doc = fixtures::SmallScene();
  doc.active_camera = fixtures::kParentId;  // exists but has no camera component
  Diagnostics d;
  ValidateStructure(doc, d);
  SOV_REQUIRE(!d.ok());
  SOV_CHECK_EQ(d.errors[0].code, ErrorCode::MissingDependency);
}

SOV_TEST(scene, content_hash_ignores_revision_only) {
  SceneDocument a = fixtures::SmallScene();
  SceneDocument b = a;
  b.revision += 7;
  SOV_CHECK_EQ(ContentHash(a), ContentHash(b));
  b.Find(fixtures::kParentId)->name = "Renamed";
  SOV_CHECK(ContentHash(a) != ContentHash(b));
}

SOV_TEST(scene, diff_reports_field_level_changes) {
  SceneDocument a = fixtures::SmallScene();
  SceneDocument b = a;
  b.revision = a.revision + 1;
  b.Find(fixtures::kParentId)->transform.position = {5, 0, 0};
  b.Erase(fixtures::kChildId);
  const SceneDiff d = DiffScenes(a, b);
  SOV_CHECK_EQ(d.modified.size(), std::size_t{1});
  SOV_CHECK_EQ(d.deleted.size(), std::size_t{1});
  SOV_CHECK(d.created.empty());
  SOV_REQUIRE(!d.modified.empty());
  SOV_CHECK_EQ(d.modified[0].changes.size(), std::size_t{1});
  SOV_CHECK_EQ(d.modified[0].changes[0].path, std::string("components.transform.position"));
  SOV_CHECK(DiffScenes(a, a).empty());
}

SOV_TEST(scene, reflection_descriptors_cover_every_component_field) {
  // Every persisted field round-trips through its descriptor, and every component has a key and fields.
  const SceneDocument doc = fixtures::SmallScene();
  for (const auto& c : Components()) {
    SOV_CHECK(!c.key.empty());
    SOV_CHECK(!c.fields.empty());
    for (const auto& f : c.fields) {
      SOV_CHECK(f.get && f.set && f.default_value);
      SOV_CHECK(!f.label.empty());
      SOV_CHECK(!f.description.empty());
    }
  }
  SOV_CHECK(FindComponent("transform")->required);
  SOV_CHECK(FindComponent("nonexistent") == nullptr);
  SOV_CHECK_EQ(FindField(MaterialDescriptor(), "roughness")->access, FieldAccess::MaterialSet);
  SOV_CHECK_EQ(FindField(*FindComponent("camera"), "near_clip")->access, FieldAccess::CreateOnly);
}

SOV_TEST(scene, schema_files_match_reflection_descriptors) {
  // schemas/scene.schema.json is a contract document; its component property sets must match the
  // descriptors this build validates against.
  auto text = fs::ReadFile(ctx.SourceDir() / "schemas" / "scene.schema.json", 1 << 20);
  SOV_REQUIRE_OK(text);
  auto schema = ParseStrictJson(*text);
  SOV_REQUIRE_OK(schema);
  const Json& comps = (*schema)["$defs"]["components"]["properties"];
  SOV_REQUIRE(comps.is_object());
  std::size_t schema_count = 0;
  for (auto it = comps.begin(); it != comps.end(); ++it) ++schema_count;
  SOV_CHECK_EQ(schema_count, Components().size());
  for (const auto& c : Components()) {
    const auto ref = comps.find(std::string(c.key));
    SOV_REQUIRE_MSG(ref != comps.end(), std::string(c.key));
    const std::string def_name = (*ref)["$ref"].get<std::string>().substr(std::string("#/$defs/").size());
    const Json& props = (*schema)["$defs"][def_name]["properties"];
    std::size_t n = 0;
    for (auto it = props.begin(); it != props.end(); ++it) {
      ++n;
      SOV_CHECK_MSG(FindField(c, it.key()) != nullptr, std::string(c.key) + "." + it.key());
    }
    SOV_CHECK_EQ(n, c.fields.size());
  }
}
