#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "sov/core/json.h"
#include "sov/core/result.h"
#include "sov/core/uuid.h"
#include "sov/world/reflection.h"
#include "sov/world/scene_document.h"

// Forward-edit operations (protocol sovereign.command_batch v1). Every operation names its own
// target entity; there is no batch-wide target.
namespace sov::cmd {

enum class OpKind { EntityCreate, EntityRename, TransformSet, MaterialSet, EntityDelete };

std::string_view ToString(OpKind kind);  // "entity.create", ...

struct FieldAssignment {
  const scene::FieldDescriptor* field = nullptr;
  scene::FieldValue value;
  bool operator==(const FieldAssignment& o) const { return field == o.field && value == o.value; }
};

struct Operation {
  OpKind kind = OpKind::EntityRename;
  Uuid entity_id;

  // entity.create: complete initial entity (defaults filled for omitted fields)
  std::optional<scene::EntityData> entity;
  // entity.rename
  std::string name;
  // transform.set: position/rotation/scale always; parent only when parent_specified
  scene::TransformComponent transform;
  bool parent_specified = false;
  // material.set: one or more material fields
  std::vector<FieldAssignment> material;

  bool operator==(const Operation&) const = default;

  static Operation Create(scene::EntityData entity);
  static Operation Rename(const Uuid& id, std::string name);
  static Operation SetTransform(const Uuid& id, const scene::TransformComponent& t, bool include_parent);
  static Operation SetMaterial(const Uuid& id, std::vector<FieldAssignment> fields);
  static Operation Delete(const Uuid& id);
};

// Strict parse of one operation object. `path` is the JSON pointer of the operation.
Result<Operation> ParseOperation(const Json& json, const std::string& path);
// Normalized JSON form (defaults filled, fixed key order). Used for request hashing and journals.
Json OperationToJson(const Operation& op);

}  // namespace sov::cmd
