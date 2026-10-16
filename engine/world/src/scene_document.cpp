#include "sov/world/scene_document.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "sov/core/fs.h"
#include "sov/core/sha256.h"
#include "sov/world/reflection.h"

namespace sov::scene {
namespace {

constexpr std::size_t kMaxErrors = 100;

JsonLimits SceneJsonLimits() {
  JsonLimits l;
  l.max_bytes = SceneLimits::kMaxFileBytes;
  l.max_depth = 16;
  l.max_string_bytes = 4096;
  return l;
}

bool LessById(const EntityData& a, const EntityData& b) { return a.id < b.id; }

// Reports every member of `obj` that is not in `allowed`.
void RejectUnknownKeys(const Json& obj, std::initializer_list<std::string_view> allowed, const std::string& path,
                       Diagnostics& d) {
  for (auto it = obj.begin(); it != obj.end(); ++it) {
    bool known = false;
    for (auto k : allowed) known = known || (k == it.key());
    if (!known) {
      d.Add(MakeError(ErrorCode::SchemaError, std::format("unknown field \"{}\"", it.key()),
                      JsonPointerAppend(path, it.key())));
    }
  }
}

const Json* Member(const Json& obj, std::string_view key, const std::string& path, Diagnostics& d) {
  auto it = obj.find(key);
  if (it == obj.end()) {
    d.Add(MakeError(ErrorCode::SchemaError, std::format("missing required field \"{}\"", key), path));
    return nullptr;
  }
  return &*it;
}

std::optional<Vec3> ReadVec3(const Json& j, double min, double max, const std::string& path, Diagnostics& d) {
  if (!j.is_array() || j.size() != 3) {
    d.Add(MakeError(ErrorCode::SchemaError, "expected an array of 3 numbers", path));
    return std::nullopt;
  }
  double v[3];
  for (std::size_t i = 0; i < 3; ++i) {
    if (!j[i].is_number() || !std::isfinite(j[i].get<double>())) {
      d.Add(MakeError(ErrorCode::SchemaError, "expected a finite number", JsonPointerAppend(path, i)));
      return std::nullopt;
    }
    v[i] = j[i].get<double>();
    if (v[i] < min || v[i] > max) {
      d.Add(MakeError(ErrorCode::InvalidValue, std::format("{} is outside [{}, {}]", v[i], min, max),
                      JsonPointerAppend(path, i)));
      return std::nullopt;
    }
  }
  return Vec3{v[0], v[1], v[2]};
}

void ReadComponents(const Json& j, const std::string& path, EntityData& e, Diagnostics& d) {
  if (!j.is_object()) {
    d.Add(MakeError(ErrorCode::SchemaError, "expected an object", path));
    return;
  }
  for (auto it = j.begin(); it != j.end(); ++it) {
    const std::string cpath = JsonPointerAppend(path, it.key());
    const ComponentDescriptor* c = FindComponent(it.key());
    if (!c) {
      d.Add(MakeError(ErrorCode::UnsupportedComponent,
                      std::format("component type \"{}\" is not supported by this build; refusing to drop it", it.key()),
                      cpath));
      continue;
    }
    if (!it.value().is_object()) {
      d.Add(MakeError(ErrorCode::SchemaError, "expected an object", cpath));
      continue;
    }
    c->add_default(e);
    for (auto fit = it.value().begin(); fit != it.value().end(); ++fit) {
      if (!FindField(*c, fit.key())) {
        d.Add(MakeError(ErrorCode::SchemaError, std::format("unknown field \"{}\" in {}", fit.key(), c->key),
                        JsonPointerAppend(cpath, fit.key())));
      }
    }
    for (const auto& f : c->fields) {
      auto fj = it.value().find(f.key);
      if (fj == it.value().end()) {
        d.Add(MakeError(ErrorCode::SchemaError, std::format("missing required field \"{}\"", f.key), cpath));
        continue;
      }
      auto v = ParseFieldValue(f, *fj, JsonPointerAppend(cpath, f.key));
      if (!v) {
        d.Add(v.take_error());
        continue;
      }
      f.set(e, *v);
    }
  }
  for (const auto& c : Components()) {
    if (c.required && j.find(c.key) == j.end()) {
      d.Add(MakeError(ErrorCode::SchemaError, std::format("missing required component \"{}\"", c.key), path));
    }
  }
  // Contract v1 dependentRequired: geometry and its material come together.
  if (e.primitive_mesh.has_value() != e.material.has_value()) {
    d.Add(MakeError(ErrorCode::SchemaError,
                    e.primitive_mesh ? "PrimitiveMesh requires MaterialParameters" : "MaterialParameters requires PrimitiveMesh",
                    path));
  }
  if (e.camera && e.camera->far_m <= e.camera->near_m) {
    d.Add(MakeError(ErrorCode::InvalidValue, "Camera far_m must be greater than near_m", JsonPointerAppend(path, "Camera")));
  }
  if (e.material && e.material->base_color_rgba.w < 1.0) {
    d.warnings.push_back(std::format("{}: base_color_rgba alpha {} is stored but G1 renders opaque",
                                     JsonPointerAppend(path, "MaterialParameters"), e.material->base_color_rgba.w));
  }
}

void ReadEntity(const Json& j, const std::string& path, EntityData& e, Diagnostics& d) {
  if (!j.is_object()) {
    d.Add(MakeError(ErrorCode::SchemaError, "expected an object", path));
    return;
  }
  RejectUnknownKeys(j, {"id", "name", "components"}, path, d);
  if (const Json* id = Member(j, "id", path, d)) {
    auto parsed = id->is_string() ? Uuid::Parse(id->get_ref<const std::string&>()) : std::nullopt;
    if (!parsed || parsed->IsNil()) {
      d.Add(MakeError(ErrorCode::SchemaError, "id must be a non-nil canonical lowercase UUID", JsonPointerAppend(path, "id")));
    } else {
      e.id = *parsed;
    }
  }
  if (const Json* name = Member(j, "name", path, d)) {
    if (!name->is_string()) {
      d.Add(MakeError(ErrorCode::SchemaError, "name must be a string", JsonPointerAppend(path, "name")));
    } else if (auto err = ValidateName(name->get_ref<const std::string&>())) {
      d.Add(MakeError(ErrorCode::InvalidValue, *err, JsonPointerAppend(path, "name")));
    } else {
      e.name = name->get<std::string>();
    }
  }
  if (const Json* comps = Member(j, "components", path, d)) {
    ReadComponents(*comps, JsonPointerAppend(path, "components"), e, d);
  }
}

}  // namespace

std::optional<std::size_t> Utf8CodePoints(std::string_view text) {
  std::size_t count = 0;
  for (std::size_t i = 0; i < text.size();) {
    const auto c = static_cast<unsigned char>(text[i]);
    std::size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xe ? 3 : (c >> 3) == 0x1e ? 4 : 0;
    if (len == 0 || i + len > text.size()) return std::nullopt;
    for (std::size_t k = 1; k < len; ++k) {
      if ((static_cast<unsigned char>(text[i + k]) >> 6) != 0x2) return std::nullopt;
    }
    i += len;
    ++count;
  }
  return count;
}

// ---------------------------------------------------------------------------------------------

const EntityData* SceneDocument::Find(const Uuid& id) const {
  auto it = std::lower_bound(entities.begin(), entities.end(), id,
                             [](const EntityData& e, const Uuid& key) { return e.id < key; });
  return (it != entities.end() && it->id == id) ? &*it : nullptr;
}

EntityData* SceneDocument::Find(const Uuid& id) {
  return const_cast<EntityData*>(static_cast<const SceneDocument*>(this)->Find(id));
}

bool SceneDocument::Insert(EntityData entity) {
  auto it = std::lower_bound(entities.begin(), entities.end(), entity, LessById);
  if (it != entities.end() && it->id == entity.id) return false;
  entities.insert(it, std::move(entity));
  return true;
}

bool SceneDocument::Erase(const Uuid& id) {
  auto it = std::lower_bound(entities.begin(), entities.end(), id,
                             [](const EntityData& e, const Uuid& key) { return e.id < key; });
  if (it == entities.end() || it->id != id) return false;
  entities.erase(it);
  return true;
}

void Diagnostics::Add(Error e) {
  if (errors.size() < kMaxErrors) errors.push_back(std::move(e));
}

Error Diagnostics::First() const {
  return errors.empty() ? MakeError(ErrorCode::Internal, "no error recorded") : errors.front();
}

std::string_view ToString(PrimitiveShape shape) {
  switch (shape) {
    case PrimitiveShape::Box: return "box";
    case PrimitiveShape::Plane: return "plane";
    case PrimitiveShape::Cylinder: return "cylinder";
  }
  return "box";
}

std::optional<PrimitiveShape> ParsePrimitiveShape(std::string_view text) {
  if (text == "box") return PrimitiveShape::Box;
  if (text == "plane") return PrimitiveShape::Plane;
  if (text == "cylinder") return PrimitiveShape::Cylinder;
  return std::nullopt;
}

std::optional<std::string> ValidateName(std::string_view name) {
  if (name.empty()) return std::string("name must not be empty");
  const auto chars = Utf8CodePoints(name);
  if (!chars) return std::string("name is not valid UTF-8");
  if (*chars > SceneLimits::kMaxNameChars) {
    return std::format("name is {} characters; limit is {}", *chars, SceneLimits::kMaxNameChars);
  }
  for (unsigned char c : name) {
    if (c < 0x20 || c == 0x7f) return std::string("name must not contain control characters");
  }
  return std::nullopt;
}

int HierarchyDepth(const SceneDocument& doc, const Uuid& id) {
  int depth = 0;
  const EntityData* e = doc.Find(id);
  std::unordered_set<Uuid> seen;
  while (e && e->transform.parent) {
    if (!seen.insert(e->id).second) return -1;
    e = doc.Find(*e->transform.parent);
    if (!e) return -1;
    if (++depth > SceneLimits::kMaxHierarchyDepth + 1) return depth;
  }
  return e ? depth : -1;
}

void ValidateStructure(const SceneDocument& doc, Diagnostics& d) {
  if (doc.entities.size() > SceneLimits::kMaxEntities) {
    d.Add(MakeError(ErrorCode::LimitExceeded,
                    std::format("{} entities; limit is {}", doc.entities.size(), SceneLimits::kMaxEntities), "/entities"));
  }
  if (doc.entities.empty()) {
    d.Add(MakeError(ErrorCode::SchemaError, "a scene needs at least one entity (its active camera)", "/entities"));
  }
  for (std::size_t i = 1; i < doc.entities.size(); ++i) {
    if (doc.entities[i - 1].id == doc.entities[i].id) {
      Error e = MakeError(ErrorCode::DuplicateEntity,
                          std::format("entity id {} appears more than once", doc.entities[i].id.ToString()), "/entities");
      e.entity_id = doc.entities[i].id.ToString();
      d.Add(std::move(e));
    }
  }
  for (const auto& e : doc.entities) {
    if (e.transform.parent) {
      if (*e.transform.parent == e.id) {
        Error err = MakeError(ErrorCode::CycleDetected, "entity cannot be its own parent");
        err.entity_id = e.id.ToString();
        d.Add(std::move(err));
        continue;
      }
      if (!doc.Find(*e.transform.parent)) {
        Error err = MakeError(ErrorCode::MissingDependency,
                              std::format("parent {} does not exist", e.transform.parent->ToString()));
        err.entity_id = e.id.ToString();
        d.Add(std::move(err));
        continue;
      }
      const int depth = HierarchyDepth(doc, e.id);
      if (depth < 0) {
        Error err = MakeError(ErrorCode::CycleDetected, "parent chain forms a cycle");
        err.entity_id = e.id.ToString();
        d.Add(std::move(err));
      } else if (depth > SceneLimits::kMaxHierarchyDepth) {
        Error err = MakeError(ErrorCode::LimitExceeded,
                              std::format("hierarchy depth exceeds {}", SceneLimits::kMaxHierarchyDepth));
        err.entity_id = e.id.ToString();
        d.Add(std::move(err));
      }
    }
  }
  const EntityData* cam = doc.Find(doc.active_camera_id);
  if (!cam) {
    d.Add(MakeError(ErrorCode::MissingDependency,
                    std::format("active_camera_id {} does not name an entity in this scene", doc.active_camera_id.ToString()),
                    "/active_camera_id"));
  } else if (!cam->camera) {
    d.Add(MakeError(ErrorCode::MissingDependency,
                    std::format("active_camera_id {} has no Camera component", doc.active_camera_id.ToString()),
                    "/active_camera_id"));
  }
}

Result<SceneDocument> ParseSceneDocument(const Json& j, Diagnostics* diagnostics) {
  Diagnostics local;
  Diagnostics& d = diagnostics ? *diagnostics : local;
  SceneDocument doc;

  if (!j.is_object()) {
    d.Add(MakeError(ErrorCode::SchemaError, "scene document must be a JSON object", ""));
    return d.First();
  }

  // Version first: a document we cannot interpret must not be partially read.
  const auto ver = j.find("schema_version");
  if (ver == j.end() || !ver->is_number_integer()) {
    d.Add(MakeError(ErrorCode::SchemaError, "\"schema_version\" must be an integer", "/schema_version"));
    return d.First();
  }
  if (ver->get<std::int64_t>() != 1) {
    d.Add(MakeError(ErrorCode::UnknownSchemaVersion,
                    std::format("scene schema_version {} is not supported by this build (supports 1)",
                                ver->get<std::int64_t>()),
                    "/schema_version"));
    return d.First();
  }
  doc.schema_version = 1;

  RejectUnknownKeys(j, {"schema_version", "scene_id", "name", "revision", "active_camera_id", "background_linear_rgb",
                        "entities"},
                    "", d);

  const auto uuid_member = [&](std::string_view key, Uuid& out) {
    if (const Json* id = Member(j, key, "", d)) {
      auto parsed = id->is_string() ? Uuid::Parse(id->get_ref<const std::string&>()) : std::nullopt;
      if (!parsed || parsed->IsNil()) {
        d.Add(MakeError(ErrorCode::SchemaError, std::format("{} must be a non-nil canonical lowercase UUID", key),
                        JsonPointerAppend("", key)));
      } else {
        out = *parsed;
      }
    }
  };
  uuid_member("scene_id", doc.scene_id);
  if (const Json* name = Member(j, "name", "", d)) {
    if (!name->is_string()) {
      d.Add(MakeError(ErrorCode::SchemaError, "name must be a string", "/name"));
    } else if (auto err = ValidateName(name->get_ref<const std::string&>())) {
      d.Add(MakeError(ErrorCode::InvalidValue, *err, "/name"));
    } else {
      doc.name = name->get<std::string>();
    }
  }
  if (const Json* rev = Member(j, "revision", "", d)) {
    if (!rev->is_number_unsigned()) {
      d.Add(MakeError(ErrorCode::SchemaError, "revision must be a non-negative integer", "/revision"));
    } else {
      doc.revision = rev->get<std::uint64_t>();
    }
  }
  uuid_member("active_camera_id", doc.active_camera_id);
  if (const Json* bg = Member(j, "background_linear_rgb", "", d)) {
    if (auto v = ReadVec3(*bg, 0.0, 1.0, "/background_linear_rgb", d)) doc.background_linear_rgb = *v;
  }

  bool was_sorted = true;
  if (const Json* entities = Member(j, "entities", "", d)) {
    if (!entities->is_array()) {
      d.Add(MakeError(ErrorCode::SchemaError, "entities must be an array", "/entities"));
    } else if (entities->size() > SceneLimits::kMaxEntities) {
      d.Add(MakeError(ErrorCode::LimitExceeded,
                      std::format("{} entities; limit is {}", entities->size(), SceneLimits::kMaxEntities),
                      "/entities"));
    } else {
      doc.entities.reserve(entities->size());
      for (std::size_t i = 0; i < entities->size(); ++i) {
        EntityData e;
        ReadEntity((*entities)[i], JsonPointerAppend("/entities", i), e, d);
        if (!doc.entities.empty() && !(doc.entities.back().id < e.id)) was_sorted = false;
        doc.entities.push_back(std::move(e));
      }
      std::stable_sort(doc.entities.begin(), doc.entities.end(), LessById);
    }
  }
  if (!was_sorted) d.warnings.push_back("entities are not in canonical (ascending id) order; they will be reordered on save");

  if (d.ok()) ValidateStructure(doc, d);
  if (!d.ok()) return d.First();
  return doc;
}

Result<EntityData> ParseEntity(const Json& json, const std::string& path) {
  Diagnostics d;
  EntityData e;
  ReadEntity(json, path, e, d);
  if (!d.ok()) return d.First();
  return e;
}

Result<SceneDocument> ParseSceneText(std::string_view text, Diagnostics* diagnostics) {
  auto json = ParseStrictJson(text, SceneJsonLimits());
  if (!json) {
    if (diagnostics) diagnostics->Add(json.error());
    return json.take_error();
  }
  return ParseSceneDocument(*json, diagnostics);
}

Json ComponentsToJson(const EntityData& e) {
  Json comps = Json::object();
  for (const auto& c : Components()) {
    if (!c.has(e)) continue;
    Json fields = Json::object();
    for (const auto& f : c.fields) {
      if (f.persisted) fields[std::string(f.key)] = FieldValueToJson(f, f.get(e));
    }
    comps[std::string(c.key)] = std::move(fields);
  }
  return comps;
}

Json EntityToJson(const EntityData& e) {
  Json j = Json::object();
  j["id"] = e.id.ToString();
  j["name"] = e.name;
  j["components"] = ComponentsToJson(e);
  return j;
}

Json SceneToJson(const SceneDocument& doc) {
  Json j = Json::object();
  j["schema_version"] = doc.schema_version;
  j["scene_id"] = doc.scene_id.ToString();
  j["name"] = doc.name;
  j["revision"] = doc.revision;
  j["active_camera_id"] = doc.active_camera_id.ToString();
  const auto& bg = doc.background_linear_rgb;
  j["background_linear_rgb"] = Json::array({bg.x, bg.y, bg.z});
  Json entities = Json::array();
  for (const auto& e : doc.entities) entities.push_back(EntityToJson(e));
  j["entities"] = std::move(entities);
  return j;
}

std::string SerializeScene(const SceneDocument& doc) { return CanonicalJson(SceneToJson(doc)); }

std::string ContentHash(const SceneDocument& doc) {
  SceneDocument copy = doc;
  copy.revision = 0;
  return Sha256Hex(SerializeScene(copy));
}

std::string ProvenanceLabel(const SceneDocument& doc, const std::filesystem::path& file) {
  return std::format("{} ({}, revision {}, content sha256 {})", doc.name, file.filename().string(), doc.revision,
                     ContentHash(doc));
}

Result<LoadedScene> LoadSceneFile(const std::filesystem::path& path) {
  auto bytes = fs::ReadFile(path, SceneLimits::kMaxFileBytes);
  if (!bytes) return bytes.take_error();
  Diagnostics d;
  auto doc = ParseSceneText(*bytes, &d);
  if (!doc) return doc.take_error();
  LoadedScene out{std::move(doc).value(), std::move(bytes).value(), {}, std::move(d.warnings)};
  out.file_sha256 = Sha256Hex(out.file_bytes);
  return out;
}

SceneDocument MakeNewScene(const Uuid& scene_id, std::string name, const Uuid& camera_id) {
  SceneDocument doc;
  doc.scene_id = scene_id;
  doc.name = std::move(name);
  doc.revision = 0;
  EntityData cam;
  cam.id = camera_id;
  cam.name = "Main Camera";
  cam.transform.position = {0, 2, 8};
  cam.camera.emplace();
  doc.Insert(std::move(cam));
  doc.active_camera_id = camera_id;
  return doc;
}

}  // namespace sov::scene
