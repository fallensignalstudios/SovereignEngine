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

// Forward-edit operations of the owner's command batch contract v1
// (schemas/command_batch.schema.json). Every operation names its own target entity; there is no
// batch-wide target. Wire shapes:
//   entity.create  {op, entity: <complete Entity>}
//   entity.rename  {op, entity_id, name}
//   transform.set  {op, entity_id, transform: <complete Transform incl. parent_id>}
//   material.set   {op, entity_id, material: <complete MaterialParameters>}
//   entity.delete  {op, entity_id}
namespace sov::cmd {

enum class OpKind { EntityCreate, EntityRename, TransformSet, MaterialSet, EntityDelete };

std::string_view ToString(OpKind kind);  // "entity.create", ...

struct Operation {
  OpKind kind = OpKind::EntityRename;
  Uuid entity_id;

  // entity.create: the complete entity
  std::optional<scene::EntityData> entity;
  // entity.rename
  std::string name;
  // transform.set: complete replacement, including the parent
  scene::TransformComponent transform;
  // material.set: complete replacement
  scene::MaterialComponent material;

  bool operator==(const Operation&) const = default;

  static Operation Create(scene::EntityData entity);
  static Operation Rename(const Uuid& id, std::string name);
  static Operation SetTransform(const Uuid& id, const scene::TransformComponent& t);
  static Operation SetMaterial(const Uuid& id, const scene::MaterialComponent& material);
  static Operation Delete(const Uuid& id);
};

// Strict parse of one operation object. `path` is the JSON pointer of the operation.
Result<Operation> ParseOperation(const Json& json, const std::string& path);
// Normalized JSON form (contract wire shape, fixed key order). Used for request hashing and journals.
Json OperationToJson(const Operation& op);

}  // namespace sov::cmd
