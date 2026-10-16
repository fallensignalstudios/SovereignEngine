#include <format>
#include <functional>

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

// SmallScene entity order (ascending id): 0 parent box, 1 child cylinder, 2 marker, 3 camera.
constexpr int kBox = 0, kCylinder = 1, kCamera = 3;

std::string Pointer(int entity, const char* rest) { return std::format("/entities/{}/{}", entity, rest); }

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

SOV_TEST(scene, packet_first_light_reads_and_is_the_canonical_scene) {
  // The owner's fixture (examples/, verbatim from the packet) and the canonical authored scene
  // (content/authoring/, created from it by `sovereign-cli scene import`) are the same document.
  auto packet = LoadSceneFile(ctx.SourceDir() / "examples/first_light.scene.json");
  SOV_REQUIRE_OK(packet);
  auto canonical = LoadSceneFile(ctx.SourceDir() / "content/authoring/first_light.scene.json");
  SOV_REQUIRE_OK(canonical);
  SOV_CHECK(packet->document == canonical->document);
  SOV_CHECK_EQ(packet->document.revision, std::uint64_t{0});
  SOV_CHECK_EQ(packet->document.entities.size(), std::size_t{12});
  // IDs, transforms, camera and materials survive save/load exactly (G1-D01).
  SOV_CHECK_EQ(SerializeScene(packet->document), canonical->file_bytes);
  const EntityData* cam = packet->document.Find(packet->document.active_camera_id);
  SOV_REQUIRE(cam && cam->camera);
  SOV_CHECK_EQ(cam->camera->fov_y_degrees, 60.0);
  SOV_CHECK_EQ(cam->transform.rotation.x, -0.13052619222005157);
  const EntityData* strip = packet->document.Find(*Uuid::Parse("10000000-0000-4000-8000-000000000130"));
  SOV_REQUIRE(strip && strip->material);
  SOV_CHECK(strip->material->emissive_rgb == Vec3(1.4, 0.45, 0.06));
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
  j["entities"][0]["components"]["Transform"]["positon"] = Json::array({0, 0, 0});
  Error e = ParseExpectingError(j);
  SOV_CHECK_EQ(e.code, ErrorCode::SchemaError);
  SOV_CHECK_EQ(e.path, std::string("/entities/0/components/Transform/positon"));

  j = SceneToJson(fixtures::SmallScene());
  j["entities"][1]["tags"] = Json::array();
  e = ParseExpectingError(j);
  SOV_CHECK_EQ(e.code, ErrorCode::SchemaError);
  SOV_CHECK_EQ(e.path, std::string("/entities/1/tags"));

  j = SceneToJson(fixtures::SmallScene());
  j["lighting_rig"] = "studio";
  e = ParseExpectingError(j);
  SOV_CHECK_EQ(e.path, std::string("/lighting_rig"));

  // Fields of the earlier reconstructed format are unknown to contract v1, not silently accepted.
  for (const char* legacy : {"format", "environment", "region", "asset_references", "active_camera"}) {
    j = SceneToJson(fixtures::SmallScene());
    j[legacy] = Json::object();
    SOV_CHECK_MSG(ParseExpectingError(j).path == std::string("/") + legacy, legacy);
  }
  j = SceneToJson(fixtures::SmallScene());
  j["entities"][kBox]["components"]["MaterialParameters"]["roughness"] = 0.5;
  SOV_CHECK_EQ(ParseExpectingError(j).code, ErrorCode::SchemaError);
}

SOV_TEST(scene, unknown_component_types_are_rejected) {
  Json j = SceneToJson(fixtures::SmallScene());
  j["entities"][0]["components"]["RigidBody"] = Json{{"mass", 10}};
  Error e = ParseExpectingError(j);
  SOV_CHECK_EQ(e.code, ErrorCode::UnsupportedComponent);
  SOV_CHECK_EQ(e.path, std::string("/entities/0/components/RigidBody"));
  // Component keys are case-sensitive contract names.
  j = SceneToJson(fixtures::SmallScene());
  j["entities"][0]["components"]["transform"] = j["entities"][0]["components"]["Transform"];
  SOV_CHECK_EQ(ParseExpectingError(j).code, ErrorCode::UnsupportedComponent);
}

SOV_TEST(scene, unknown_schema_version_fails_before_reading) {
  Json j = SceneToJson(fixtures::SmallScene());
  j["schema_version"] = 2;
  j["entities"][0]["future_field"] = true;  // would also be an error; version must be reported first
  Error e = ParseExpectingError(j);
  SOV_CHECK_EQ(e.code, ErrorCode::UnknownSchemaVersion);
  SOV_CHECK_EQ(e.path, std::string("/schema_version"));
  j = SceneToJson(fixtures::SmallScene());
  j.erase("schema_version");
  SOV_CHECK_EQ(ParseExpectingError(j).code, ErrorCode::SchemaError);
}

SOV_TEST(scene, malformed_and_nonfinite_input_fail_with_location) {
  // Malformed JSON reports a byte offset; non-finite numbers are rejected by the strict reader.
  auto bad = ParseSceneText("{\"schema_version\": 1,\n  \"scene_id\": oops}");
  SOV_REQUIRE(!bad.ok());
  SOV_CHECK_EQ(bad.error().code, ErrorCode::SchemaError);
  SOV_CHECK_MSG(bad.error().message.find("byte") != std::string::npos ||
                    bad.error().message.find("line") != std::string::npos,
                bad.error().message);
  std::string text = SerializeScene(fixtures::SmallScene());
  text.replace(text.find("[1.0, 0.5, -2.0]"), 16, "[NaN, 0.5, -2.0]");
  SOV_CHECK(!ParseSceneText(text).ok());
  text = SerializeScene(fixtures::SmallScene());
  text.replace(text.find("[1.0, 0.5, -2.0]"), 16, "[1e999, 0.5, -2.0]");
  SOV_CHECK(!ParseSceneText(text).ok());
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
  const auto expect = [&](const std::string& pointer_path, Json value, ErrorCode code, std::string reported_at = {}) {
    Json j = SceneToJson(fixtures::SmallScene());
    j[Json::json_pointer(pointer_path)] = std::move(value);
    const Error e = ParseExpectingError(j);
    if (reported_at.empty()) reported_at = pointer_path;
    SOV_CHECK_MSG(e.code == code, pointer_path + " -> " + e.Describe());
    SOV_CHECK_MSG(e.path.rfind(reported_at, 0) == 0, pointer_path + " reported at " + e.path);
  };
  const std::string rot = Pointer(kBox, "components/Transform/rotation_xyzw");
  expect(rot, Json::array({0, 0, 0, 2}), ErrorCode::InvalidValue);              // component outside [-1, 1]
  expect(rot, Json::array({0, 0, 0, 0}), ErrorCode::InvalidValue);              // unnormalized
  expect(rot, Json::array({0.7071, 0, 0, 0.7071}), ErrorCode::InvalidValue);   // |q|^2 = 0.99998
  expect(Pointer(kBox, "components/Transform/scale"), Json::array({1, 0, 1}), ErrorCode::InvalidValue);
  expect(Pointer(kBox, "components/Transform/position"), Json::array({1, 2}), ErrorCode::SchemaError);
  expect(Pointer(kBox, "components/Transform/position"), Json::array({1.000001e6, 0, 0}), ErrorCode::InvalidValue);
  expect(Pointer(kBox, "components/Transform/parent_id"), Json("not-a-uuid"), ErrorCode::SchemaError);
  expect(Pointer(kCylinder, "components/MaterialParameters/base_color_rgba"), Json::array({0.5, 0.5, 0.5, 1.5}),
         ErrorCode::InvalidValue);
  expect(Pointer(kCylinder, "components/MaterialParameters/base_color_rgba"), Json::array({0.5, "red", 0.5, 1}),
         ErrorCode::SchemaError);
  expect(Pointer(kCylinder, "components/MaterialParameters/base_color_rgba"), Json::array({0.5, 0.5, 0.5}),
         ErrorCode::SchemaError);
  expect(Pointer(kCylinder, "components/MaterialParameters/emissive_rgb"), Json::array({1001, 0, 0}),
         ErrorCode::InvalidValue);
  expect(Pointer(kCylinder, "components/PrimitiveMesh/primitive"), Json("torus"), ErrorCode::InvalidValue);
  expect(Pointer(kCamera, "components/Camera/projection"), Json("orthographic"), ErrorCode::InvalidValue);
  expect(Pointer(kCamera, "components/Camera/fov_y_degrees"), Json(0.5), ErrorCode::InvalidValue);
  expect(Pointer(kCamera, "components/Camera/near_m"), Json(0.0), ErrorCode::InvalidValue);  // exclusive minimum
  expect(Pointer(kCamera, "components/Camera/far_m"), Json(0.05), ErrorCode::InvalidValue,  // not beyond near_m
         Pointer(kCamera, "components/Camera"));
  expect("/background_linear_rgb", Json::array({1.5, 0, 0}), ErrorCode::InvalidValue);
  expect(Pointer(kBox, "name"), Json(""), ErrorCode::InvalidValue);
  expect(Pointer(kBox, "name"), Json("bad\nname"), ErrorCode::InvalidValue);
  expect(Pointer(kBox, "name"), Json(std::string(161, 'n')), ErrorCode::InvalidValue);
  expect(Pointer(kBox, "id"), Json("NOT-A-UUID"), ErrorCode::SchemaError);
  expect("/revision", Json(-1), ErrorCode::SchemaError);
  expect("/active_camera_id", Json(nullptr), ErrorCode::SchemaError);

  // Name limits count Unicode characters, as JSON Schema maxLength does: 160 x "é" (320 bytes) is fine.
  std::string long_name;
  for (int i = 0; i < 160; ++i) long_name += "\xc3\xa9";
  Json j = SceneToJson(fixtures::SmallScene());
  j["entities"][kBox]["name"] = long_name;
  SOV_CHECK(ParseSceneDocument(j).ok());
  // A translucent alpha is accepted (contract range) with a warning: G1 renders opaque.
  j = SceneToJson(fixtures::SmallScene());
  j["entities"][kBox]["components"]["MaterialParameters"]["base_color_rgba"][3] = 0.5;
  Diagnostics d;
  SOV_CHECK(ParseSceneDocument(j, &d).ok());
  SOV_CHECK_EQ(d.warnings.size(), std::size_t{1});
}

SOV_TEST(scene, missing_required_fields_rejected) {
  const auto expect_schema_error = [&](const std::function<void(Json&)>& mutate, const char* what) {
    Json j = SceneToJson(fixtures::SmallScene());
    mutate(j);
    SOV_CHECK_MSG(ParseExpectingError(j).code == ErrorCode::SchemaError, what);
  };
  expect_schema_error([](Json& j) { j["entities"][0]["components"]["Transform"].erase("scale"); }, "scale");
  expect_schema_error([](Json& j) { j["entities"][0]["components"]["Transform"].erase("parent_id"); }, "parent_id");
  expect_schema_error([](Json& j) { j["entities"][0]["components"].erase("Transform"); }, "Transform");
  expect_schema_error([](Json& j) { j["entities"][kBox]["components"].erase("MaterialParameters"); },
                      "PrimitiveMesh without MaterialParameters");
  expect_schema_error([](Json& j) { j["entities"][kBox]["components"].erase("PrimitiveMesh"); },
                      "MaterialParameters without PrimitiveMesh");
  expect_schema_error([](Json& j) { j["entities"][kCamera]["components"]["Camera"].erase("projection"); }, "projection");
  expect_schema_error([](Json& j) { j.erase("active_camera_id"); }, "active_camera_id");
  expect_schema_error([](Json& j) { j.erase("background_linear_rgb"); }, "background_linear_rgb");
  expect_schema_error([](Json& j) { j["entities"] = Json::array(); }, "empty entities");
}

SOV_TEST(scene, active_camera_must_reference_a_camera) {
  SceneDocument doc = fixtures::SmallScene();
  doc.active_camera_id = fixtures::kParentId;  // exists but has no Camera component
  Diagnostics d;
  ValidateStructure(doc, d);
  SOV_REQUIRE(!d.ok());
  SOV_CHECK_EQ(d.errors[0].code, ErrorCode::MissingDependency);
  SOV_CHECK_EQ(d.errors[0].path, std::string("/active_camera_id"));
  doc.active_camera_id = *Uuid::Parse("10000000-0000-4000-8000-000000999999");  // does not exist
  d = {};
  ValidateStructure(doc, d);
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
  SOV_CHECK_EQ(d.modified[0].changes[0].path, std::string("components.Transform.position"));
  SOV_CHECK(DiffScenes(a, a).empty());
}

SOV_TEST(scene, reflection_descriptors_cover_every_component_field) {
  // Every persisted field round-trips through its descriptor, and every component has a key and fields.
  for (const auto& c : Components()) {
    SOV_CHECK(!c.key.empty());
    SOV_CHECK(!c.fields.empty());
    for (const auto& f : c.fields) {
      SOV_CHECK(f.get && f.set && f.default_value);
      SOV_CHECK(!f.label.empty());
      SOV_CHECK(!f.description.empty());
    }
  }
  SOV_CHECK(FindComponent("Transform")->required);
  SOV_CHECK(FindComponent("nonexistent") == nullptr);
  SOV_CHECK_EQ(FindField(MaterialDescriptor(), "emissive_rgb")->access, FieldAccess::MaterialSet);
  SOV_CHECK_EQ(FindField(*FindComponent("Camera"), "near_m")->access, FieldAccess::CreateOnly);
  SOV_CHECK_EQ(FindField(TransformDescriptor(), "parent_id")->access, FieldAccess::TransformSet);
}

SOV_TEST(scene, schema_files_match_reflection_descriptors) {
  // schemas/scene.schema.json is the owner's contract (verbatim from the packet). Component and
  // field names, requiredness, numeric ranges and enumerations must match the descriptors this
  // build validates with, so the engine can neither be looser nor silently stricter on a value.
  auto text = fs::ReadFile(ctx.SourceDir() / "schemas" / "scene.schema.json", 1 << 20);
  SOV_REQUIRE_OK(text);
  auto schema = ParseStrictJson(*text);
  SOV_REQUIRE_OK(schema);
  const Json& defs = (*schema)["$defs"];
  const Json& comps = defs["Components"]["properties"];
  SOV_REQUIRE(comps.is_object());
  SOV_CHECK_EQ(comps.size(), Components().size());
  for (const auto& c : Components()) {
    const auto ref = comps.find(std::string(c.key));
    SOV_REQUIRE_MSG(ref != comps.end(), std::string(c.key));
    const std::string def_name = (*ref)["$ref"].get<std::string>().substr(std::string("#/$defs/").size());
    const Json& def = defs[def_name];
    const Json& props = def["properties"];
    SOV_CHECK_EQ(props.size(), c.fields.size());
    SOV_CHECK_EQ(def["required"].size(), c.fields.size());  // every field required (complete components)
    SOV_CHECK_EQ(def["additionalProperties"].get<bool>(), false);
    std::size_t i = 0;
    for (auto it = props.begin(); it != props.end(); ++it, ++i) {
      const std::string where = std::string(c.key) + "." + it.key();
      const FieldDescriptor* f = FindField(c, it.key());
      SOV_REQUIRE_MSG(f != nullptr, where);
      SOV_CHECK_MSG(&c.fields[i] == f, where + ": canonical field order differs from the schema");
      const Json& p = it.value().contains("items") ? it.value()["items"] : it.value();
      if (p.contains("minimum")) SOV_CHECK_MSG(p["minimum"].get<double>() == f->min && !f->exclusive_min, where);
      if (p.contains("exclusiveMinimum")) {
        SOV_CHECK_MSG(p["exclusiveMinimum"].get<double>() == f->min && f->exclusive_min, where);
      }
      if (p.contains("maximum")) SOV_CHECK_MSG(p["maximum"].get<double>() == f->max, where);
      if (it.value().contains("minItems")) {
        const std::size_t n = it.value()["minItems"].get<std::size_t>();
        const std::size_t want = f->type == FieldType::Vec3 || f->type == FieldType::Color ? 3 : 4;
        SOV_CHECK_MSG(n == want, where);
      }
      const Json* names = p.contains("enum") ? &p["enum"] : nullptr;
      if (names) {
        SOV_CHECK_MSG(names->size() == f->enum_values.size(), where);
        for (std::size_t k = 0; k < f->enum_values.size() && k < names->size(); ++k) {
          SOV_CHECK_MSG((*names)[k].get<std::string>() == f->enum_values[k], where);
        }
      }
      if (p.contains("const")) {
        SOV_CHECK_MSG(f->enum_values.size() == 1 && p["const"].get<std::string>() == f->enum_values[0], where);
      }
    }
  }
  // Top-level and entity envelopes.
  std::vector<std::string> top;
  for (auto it = (*schema)["properties"].begin(); it != (*schema)["properties"].end(); ++it) top.push_back(it.key());
  const Json canonical = SceneToJson(fixtures::SmallScene());
  std::vector<std::string> written;
  for (auto it = canonical.begin(); it != canonical.end(); ++it) written.push_back(it.key());
  SOV_CHECK(top == written);  // same keys, same order as the canonical writer
  SOV_CHECK_EQ((*schema)["properties"]["name"]["maxLength"].get<std::size_t>(), SceneLimits::kMaxNameChars);
  SOV_CHECK_EQ(defs["Entity"]["properties"]["name"]["maxLength"].get<std::size_t>(), SceneLimits::kMaxNameChars);
  SOV_CHECK_EQ((*schema)["properties"]["entities"]["maxItems"].get<std::size_t>(), SceneLimits::kMaxEntities);
  SOV_CHECK_EQ(defs["Components"]["dependentRequired"]["PrimitiveMesh"][0].get<std::string>(),
               std::string("MaterialParameters"));
}
