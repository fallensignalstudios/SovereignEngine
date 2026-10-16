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

// Parses one complete component object against its descriptor (all fields required, unknown
// fields rejected), writing into `scratch`.
Result<void> ParseWholeComponent(const scene::ComponentDescriptor& c, const Json& j, const std::string& path,
                                 scene::EntityData& scratch) {
  if (!j.is_object()) return At(ErrorCode::SchemaError, std::format("{} must be an object", c.key), path);
  for (auto it = j.begin(); it != j.end(); ++it) {
    if (!scene::FindField(c, it.key())) {
      return At(ErrorCode::SchemaError, std::format("unknown field \"{}\" in {}", it.key(), c.key), JsonPointerAppend(path, it.key()));
    }
  }
  for (const auto& f : c.fields) {
    auto it = j.find(f.key);
    if (it == j.end()) {
      return At(ErrorCode::SchemaError, std::format("{} requires \"{}\" (complete replacement)", c.key, f.key), path);
    }
    auto v = scene::ParseFieldValue(f, *it, JsonPointerAppend(path, f.key));
    if (!v) return v.take_error();
    f.set(scratch, *v);
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

Operation Operation::SetTransform(const Uuid& id, const scene::TransformComponent& t) {
  Operation op;
  op.kind = OpKind::TransformSet;
  op.entity_id = id;
  op.transform = t;
  return op;
}

Operation Operation::SetMaterial(const Uuid& id, const scene::MaterialComponent& material) {
  Operation op;
  op.kind = OpKind::MaterialSet;
  op.entity_id = id;
  op.material = material;
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

  if (kind == "entity.create") {
    if (auto r = RejectUnknown(j, {"op", "entity"}, path); !r) return r.take_error();
    auto it = j.find("entity");
    if (it == j.end()) return At(ErrorCode::SchemaError, "entity.create requires a complete \"entity\" object", path);
    auto e = scene::ParseEntity(*it, JsonPointerAppend(path, "entity"));
    if (!e) return e.take_error();
    return Operation::Create(std::move(e).value());
  }
  auto id = ParseId(j, "entity_id", path);
  if (!id) return id.take_error();
  if (kind == "entity.rename") {
    if (auto r = RejectUnknown(j, {"op", "entity_id", "name"}, path); !r) return r.take_error();
    auto name = ParseName(j, path);
    if (!name) return name.take_error();
    return Operation::Rename(*id, *name);
  }
  if (kind == "transform.set") {
    if (auto r = RejectUnknown(j, {"op", "entity_id", "transform"}, path); !r) return r.take_error();
    auto t = j.find("transform");
    if (t == j.end()) return At(ErrorCode::SchemaError, "transform.set requires a \"transform\" object", path);
    scene::EntityData scratch;
    if (auto r = ParseWholeComponent(scene::TransformDescriptor(), *t, JsonPointerAppend(path, "transform"), scratch); !r) {
      return r.take_error();
    }
    return Operation::SetTransform(*id, scratch.transform);
  }
  if (kind == "material.set") {
    if (auto r = RejectUnknown(j, {"op", "entity_id", "material"}, path); !r) return r.take_error();
    auto m = j.find("material");
    if (m == j.end()) return At(ErrorCode::SchemaError, "material.set requires a \"material\" object", path);
    scene::EntityData scratch;
    scratch.material.emplace();
    if (auto r = ParseWholeComponent(scene::MaterialDescriptor(), *m, JsonPointerAppend(path, "material"), scratch); !r) {
      return r.take_error();
    }
    return Operation::SetMaterial(*id, *scratch.material);
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
  switch (op.kind) {
    case OpKind::EntityCreate:
      j["entity"] = scene::EntityToJson(*op.entity);
      break;
    case OpKind::EntityRename:
      j["entity_id"] = op.entity_id.ToString();
      j["name"] = op.name;
      break;
    case OpKind::TransformSet: {
      j["entity_id"] = op.entity_id.ToString();
      scene::EntityData scratch;
      scratch.transform = op.transform;
      j["transform"] = scene::ComponentsToJson(scratch)["Transform"];
      break;
    }
    case OpKind::MaterialSet: {
      j["entity_id"] = op.entity_id.ToString();
      scene::EntityData scratch;
      scratch.primitive_mesh.emplace();
      scratch.material = op.material;
      j["material"] = scene::ComponentsToJson(scratch)["MaterialParameters"];
      break;
    }
    case OpKind::EntityDelete:
      j["entity_id"] = op.entity_id.ToString();
      break;
  }
  return j;
}

}  // namespace sov::cmd
