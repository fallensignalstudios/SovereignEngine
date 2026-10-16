#include "sov/commands/operation.h"

#include <format>

namespace sov::cmd {
namespace {

Error At(ErrorCode code, std::string message, std::string path) { return MakeError(code, std::move(message), std::move(path)); }

Result<void> RejectUnknown(const Json& obj, std::initializer_list<std::string_view> allowed, const std::string& path) {
  for (auto it = obj.begin(); it != obj.end(); ++it) {
    bool known = false;
    for (auto k : allowed) known = known || k == it.key();
    if (!known) return At(ErrorCode::SchemaError, std::format("unknown field \"{}\"", it.key()), JsonPointerAppend(path, it.key()));
  }
  return Ok();
}

Result<Uuid> ParseId(const Json& obj, std::string_view key, const std::string& path) {
  auto it = obj.find(key);
  if (it == obj.end()) return At(ErrorCode::SchemaError, std::format("missing required field \"{}\"", key), path);
  auto id = it->is_string() ? Uuid::Parse(it->get_ref<const std::string&>()) : std::nullopt;
  if (!id || id->IsNil()) {
    return At(ErrorCode::SchemaError, "expected a non-nil canonical lowercase UUID", JsonPointerAppend(path, key));
  }
  return *id;
}

Result<std::string> ParseName(const Json& obj, const std::string& path) {
  auto it = obj.find("name");
  if (it == obj.end()) return At(ErrorCode::SchemaError, "missing required field \"name\"", path);
  if (!it->is_string()) return At(ErrorCode::SchemaError, "name must be a string", JsonPointerAppend(path, "name"));
  if (auto err = scene::ValidateName(it->get_ref<const std::string&>())) {
    return At(ErrorCode::InvalidValue, *err, JsonPointerAppend(path, "name"));
  }
  return it->get<std::string>();
}

// Parses a components object for entity.create: omitted fields take descriptor defaults,
// unknown components and fields are rejected.
Result<void> ParseCreateComponents(const Json& j, const std::string& path, scene::EntityData& e) {
  if (!j.is_object()) return At(ErrorCode::SchemaError, "components must be an object", path);
  for (auto it = j.begin(); it != j.end(); ++it) {
    const std::string cpath = JsonPointerAppend(path, it.key());
    const scene::ComponentDescriptor* c = scene::FindComponent(it.key());
    if (!c) {
      return At(ErrorCode::UnsupportedComponent, std::format("component type \"{}\" is not supported", it.key()), cpath);
    }
    if (!it.value().is_object()) return At(ErrorCode::SchemaError, "component must be an object", cpath);
    c->add_default(e);
    for (auto fit = it.value().begin(); fit != it.value().end(); ++fit) {
      const scene::FieldDescriptor* f = scene::FindField(*c, fit.key());
      if (!f) {
        return At(ErrorCode::SchemaError, std::format("unknown field \"{}\" in {}", fit.key(), c->key),
                  JsonPointerAppend(cpath, fit.key()));
      }
      auto v = scene::ParseFieldValue(*f, fit.value(), JsonPointerAppend(cpath, fit.key()));
      if (!v) return v.take_error();
      f->set(e, *v);
    }
  }
  if (e.camera && e.camera->far_clip <= e.camera->near_clip) {
    return At(ErrorCode::InvalidValue, "camera far_clip must exceed near_clip", JsonPointerAppend(path, "camera"));
  }
  return Ok();
}

}  // namespace

std::string_view ToString(OpKind kind) {
  switch (kind) {
    case OpKind::EntityCreate: return "entity.create";
    case OpKind::EntityRename: return "entity.rename";
    case OpKind::TransformSet: return "transform.set";
    case OpKind::MaterialSet: return "material.set";
    case OpKind::EntityDelete: return "entity.delete";
  }
  return "entity.rename";
}

Operation Operation::Create(scene::EntityData entity) {
  Operation op;
  op.kind = OpKind::EntityCreate;
  op.entity_id = entity.id;
  op.entity = std::move(entity);
  return op;
}

Operation Operation::Rename(const Uuid& id, std::string name) {
  Operation op;
  op.kind = OpKind::EntityRename;
  op.entity_id = id;
  op.name = std::move(name);
  return op;
}

Operation Operation::SetTransform(const Uuid& id, const scene::TransformComponent& t, bool include_parent) {
  Operation op;
  op.kind = OpKind::TransformSet;
  op.entity_id = id;
  op.transform = t;
  op.parent_specified = include_parent;
  if (!include_parent) op.transform.parent.reset();
  return op;
}

Operation Operation::SetMaterial(const Uuid& id, std::vector<FieldAssignment> fields) {
  Operation op;
  op.kind = OpKind::MaterialSet;
  op.entity_id = id;
  op.material = std::move(fields);
  return op;
}

Operation Operation::Delete(const Uuid& id) {
  Operation op;
  op.kind = OpKind::EntityDelete;
  op.entity_id = id;
  return op;
}

Result<Operation> ParseOperation(const Json& j, const std::string& path) {
  if (!j.is_object()) return At(ErrorCode::SchemaError, "operation must be an object", path);
  auto kind_it = j.find("op");
  if (kind_it == j.end() || !kind_it->is_string()) {
    return At(ErrorCode::SchemaError, "operation needs a string \"op\" field", path);
  }
  const auto& kind = kind_it->get_ref<const std::string&>();
  auto id = ParseId(j, "entity_id", path);
  if (!id) return id.take_error();

  if (kind == "entity.create") {
    if (auto r = RejectUnknown(j, {"op", "entity_id", "name", "components"}, path); !r) return r.take_error();
    scene::EntityData e;
    e.id = *id;
    auto name = ParseName(j, path);
    if (!name) return name.take_error();
    e.name = *name;
    if (auto comps = j.find("components"); comps != j.end()) {
      if (auto r = ParseCreateComponents(*comps, JsonPointerAppend(path, "components"), e); !r) return r.take_error();
    }
    return Operation::Create(std::move(e));
  }
  if (kind == "entity.rename") {
    if (auto r = RejectUnknown(j, {"op", "entity_id", "name"}, path); !r) return r.take_error();
    auto name = ParseName(j, path);
    if (!name) return name.take_error();
    return Operation::Rename(*id, *name);
  }
  if (kind == "transform.set") {
    if (auto r = RejectUnknown(j, {"op", "entity_id", "transform"}, path); !r) return r.take_error();
    auto t = j.find("transform");
    const std::string tpath = JsonPointerAppend(path, "transform");
    if (t == j.end() || !t->is_object()) return At(ErrorCode::SchemaError, "transform.set needs a \"transform\" object", path);
    const scene::ComponentDescriptor& tc = *scene::FindComponent("transform");
    scene::EntityData scratch;
    bool parent_specified = false;
    for (auto it = t->begin(); it != t->end(); ++it) {
      const scene::FieldDescriptor* f = scene::FindField(tc, it.key());
      if (!f) return At(ErrorCode::SchemaError, std::format("unknown field \"{}\"", it.key()), JsonPointerAppend(tpath, it.key()));
      if (f->access != scene::FieldAccess::TransformSet) {
        return At(ErrorCode::PermissionDenied, std::format("{} cannot be written by transform.set", it.key()),
                  JsonPointerAppend(tpath, it.key()));
      }
      auto v = scene::ParseFieldValue(*f, it.value(), JsonPointerAppend(tpath, it.key()));
      if (!v) return v.take_error();
      f->set(scratch, *v);
      if (it.key() == "parent") parent_specified = true;
    }
    for (std::string_view required : {"position", "rotation", "scale"}) {
      if (t->find(required) == t->end()) {
        return At(ErrorCode::SchemaError,
                  std::format("transform.set requires complete position, rotation and scale (missing \"{}\")", required), tpath);
      }
    }
    return Operation::SetTransform(*id, scratch.transform, parent_specified);
  }
  if (kind == "material.set") {
    if (auto r = RejectUnknown(j, {"op", "entity_id", "material"}, path); !r) return r.take_error();
    auto m = j.find("material");
    const std::string mpath = JsonPointerAppend(path, "material");
    if (m == j.end() || !m->is_object() || m->empty()) {
      return At(ErrorCode::SchemaError, "material.set needs a non-empty \"material\" object", path);
    }
    std::vector<FieldAssignment> fields;
    // Assignments are kept in descriptor order so equal requests normalize identically.
    for (const auto& f : scene::MaterialDescriptor().fields) {
      auto it = m->find(f.key);
      if (it == m->end()) continue;
      if (f.access != scene::FieldAccess::MaterialSet) {
        return At(ErrorCode::PermissionDenied, std::format("{} cannot be written by material.set", f.key),
                  JsonPointerAppend(mpath, f.key));
      }
      auto v = scene::ParseFieldValue(f, *it, JsonPointerAppend(mpath, f.key));
      if (!v) return v.take_error();
      fields.push_back({&f, *v});
    }
    for (auto it = m->begin(); it != m->end(); ++it) {
      if (!scene::FindField(scene::MaterialDescriptor(), it.key())) {
        return At(ErrorCode::SchemaError, std::format("unknown material field \"{}\"", it.key()), JsonPointerAppend(mpath, it.key()));
      }
    }
    return Operation::SetMaterial(*id, std::move(fields));
  }
  if (kind == "entity.delete") {
    if (auto r = RejectUnknown(j, {"op", "entity_id"}, path); !r) return r.take_error();
    return Operation::Delete(*id);
  }
  return At(ErrorCode::SchemaError,
            std::format("unknown operation \"{}\" (supported: entity.create, entity.rename, transform.set, material.set, "
                        "entity.delete; undo/redo are history-service calls, not batch operations)",
                        kind),
            JsonPointerAppend(path, "op"));
}

Json OperationToJson(const Operation& op) {
  Json j = Json::object();
  j["op"] = ToString(op.kind);
  j["entity_id"] = op.entity_id.ToString();
  switch (op.kind) {
    case OpKind::EntityCreate:
      j["name"] = op.entity->name;
      j["components"] = scene::ComponentsToJson(*op.entity);
      break;
    case OpKind::EntityRename:
      j["name"] = op.name;
      break;
    case OpKind::TransformSet: {
      Json t = Json::object();
      if (op.parent_specified) {
        t["parent"] = op.transform.parent ? Json(op.transform.parent->ToString()) : Json(nullptr);
      }
      t["position"] = Json::array({op.transform.position.x, op.transform.position.y, op.transform.position.z});
      t["rotation"] = Json::array({op.transform.rotation.x, op.transform.rotation.y, op.transform.rotation.z, op.transform.rotation.w});
      t["scale"] = Json::array({op.transform.scale.x, op.transform.scale.y, op.transform.scale.z});
      j["transform"] = std::move(t);
      break;
    }
    case OpKind::MaterialSet: {
      Json m = Json::object();
      for (const auto& a : op.material) m[std::string(a.field->key)] = scene::FieldValueToJson(*a.field, a.value);
      j["material"] = std::move(m);
      break;
    }
    case OpKind::EntityDelete:
      break;
  }
  return j;
}

}  // namespace sov::cmd
