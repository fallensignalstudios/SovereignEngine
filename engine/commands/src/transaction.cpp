#include "sov/commands/transaction.h"

#include <algorithm>
#include <format>
#include <functional>

namespace sov::cmd {
namespace {

using scene::EntityData;
using scene::SceneDocument;

Error OpError(ErrorCode code, std::string message, int index, const Uuid& id) {
  Error e = MakeError(code, std::move(message), std::format("/operations/{}", index));
  e.op_index = index;
  e.entity_id = id.ToString();
  return e;
}

std::vector<Uuid> ChildrenOf(const SceneDocument& doc, const Uuid& id) {
  std::vector<Uuid> out;
  for (const auto& e : doc.entities) {
    if (e.transform.parent && *e.transform.parent == id) out.push_back(e.id);
  }
  return out;
}

// Height of the subtree rooted at `id` (0 for a leaf).
int SubtreeHeight(const SceneDocument& doc, const Uuid& id, int guard = 0) {
  if (guard > scene::SceneLimits::kMaxHierarchyDepth + 1) return guard;
  int h = 0;
  for (const auto& child : ChildrenOf(doc, id)) h = std::max(h, 1 + SubtreeHeight(doc, child, guard + 1));
  return h;
}

std::optional<Error> CheckParent(const SceneDocument& doc, const Uuid& entity, const std::optional<Uuid>& parent,
                                 int index) {
  if (!parent) return std::nullopt;
  if (*parent == entity) return OpError(ErrorCode::CycleDetected, "entity cannot be its own parent", index, entity);
  if (!doc.Find(*parent)) {
    return OpError(ErrorCode::MissingDependency, std::format("parent {} does not exist", parent->ToString()), index, entity);
  }
  // Walk up from the new parent; reaching `entity` means the change would create a cycle.
  int depth = 1;
  for (const EntityData* p = doc.Find(*parent); p; p = p->transform.parent ? doc.Find(*p->transform.parent) : nullptr) {
    if (p->id == entity) {
      return OpError(ErrorCode::CycleDetected,
                     std::format("parenting to {} would create a cycle", parent->ToString()), index, entity);
    }
    if (!p->transform.parent) break;
    if (++depth > scene::SceneLimits::kMaxHierarchyDepth + 1) break;
  }
  const int total = depth + SubtreeHeight(doc, entity);
  if (total > scene::SceneLimits::kMaxHierarchyDepth) {
    return OpError(ErrorCode::LimitExceeded,
                   std::format("hierarchy depth would be {}; limit is {}", total, scene::SceneLimits::kMaxHierarchyDepth),
                   index, entity);
  }
  return std::nullopt;
}

std::optional<Error> ApplyOne(SceneDocument& doc, const Operation& op, int index, std::vector<Operation>& inverse) {
  switch (op.kind) {
    case OpKind::EntityCreate: {
      if (doc.Find(op.entity_id)) {
        return OpError(ErrorCode::DuplicateEntity, std::format("entity {} already exists", op.entity_id.ToString()), index,
                       op.entity_id);
      }
      if (doc.entities.size() >= scene::SceneLimits::kMaxEntities) {
        return OpError(ErrorCode::LimitExceeded, "scene entity limit reached", index, op.entity_id);
      }
      if (auto err = CheckParent(doc, op.entity_id, op.entity->transform.parent, index)) return err;
      doc.Insert(*op.entity);
      inverse.push_back(Operation::Delete(op.entity_id));
      return std::nullopt;
    }
    case OpKind::EntityRename: {
      EntityData* e = doc.Find(op.entity_id);
      if (!e) return OpError(ErrorCode::UnknownEntity, std::format("entity {} does not exist", op.entity_id.ToString()), index, op.entity_id);
      inverse.push_back(Operation::Rename(op.entity_id, e->name));
      e->name = op.name;
      return std::nullopt;
    }
    case OpKind::TransformSet: {
      EntityData* e = doc.Find(op.entity_id);
      if (!e) return OpError(ErrorCode::UnknownEntity, std::format("entity {} does not exist", op.entity_id.ToString()), index, op.entity_id);
      // Complete replacement: parent_id is always part of transform.set.
      if (auto err = CheckParent(doc, op.entity_id, op.transform.parent, index)) return err;
      e = doc.Find(op.entity_id);
      inverse.push_back(Operation::SetTransform(op.entity_id, e->transform));
      e->transform = op.transform;
      return std::nullopt;
    }
    case OpKind::MaterialSet: {
      EntityData* e = doc.Find(op.entity_id);
      if (!e) return OpError(ErrorCode::UnknownEntity, std::format("entity {} does not exist", op.entity_id.ToString()), index, op.entity_id);
      if (!e->primitive_mesh || !e->material) {
        return OpError(ErrorCode::ComponentMissing,
                       "material.set targets an entity without geometry (PrimitiveMesh + MaterialParameters)", index,
                       op.entity_id);
      }
      inverse.push_back(Operation::SetMaterial(op.entity_id, *e->material));
      e->material = op.material;
      return std::nullopt;
    }
    case OpKind::EntityDelete: {
      const EntityData* e = doc.Find(op.entity_id);
      if (!e) return OpError(ErrorCode::UnknownEntity, std::format("entity {} does not exist", op.entity_id.ToString()), index, op.entity_id);
      const auto children = ChildrenOf(doc, op.entity_id);
      if (!children.empty()) {
        std::string list;
        for (const auto& c : children) list += (list.empty() ? "" : ", ") + c.ToString();
        return OpError(ErrorCode::MissingDependency,
                       std::format("entity has {} child(ren) ({}); delete or re-parent them earlier in the same batch. "
                                   "No cascading delete is performed.",
                                   children.size(), list),
                       index, op.entity_id);
      }
      if (doc.active_camera_id == op.entity_id) {
        return OpError(ErrorCode::MissingDependency, "entity is the scene's active camera (active_camera_id) and cannot be deleted",
                       index, op.entity_id);
      }
      inverse.push_back(Operation::Create(*e));
      doc.Erase(op.entity_id);
      return std::nullopt;
    }
  }
  return OpError(ErrorCode::Internal, "unhandled operation kind", index, op.entity_id);
}

}  // namespace

std::string_view ToString(OpStatus status) {
  switch (status) {
    case OpStatus::Applied: return "applied";
    case OpStatus::Valid: return "valid";
    case OpStatus::Failed: return "failed";
    case OpStatus::NotEvaluated: return "not_evaluated";
  }
  return "not_evaluated";
}

StageResult StageOperations(const SceneDocument& base, std::span<const Operation> ops) {
  StageResult r;
  r.staged = base;
  bool failed = false;
  for (std::size_t i = 0; i < ops.size(); ++i) {
    OpOutcome outcome{static_cast<int>(i), ops[i].kind, ops[i].entity_id, OpStatus::NotEvaluated};
    if (!failed) {
      if (auto err = ApplyOne(r.staged, ops[i], static_cast<int>(i), r.inverse)) {
        r.errors.push_back(*err);
        outcome.status = OpStatus::Failed;
        failed = true;
      } else {
        outcome.status = OpStatus::Valid;
      }
    }
    r.outcomes.push_back(outcome);
  }
  if (!failed) {
    scene::Diagnostics d;
    scene::ValidateStructure(r.staged, d);
    for (auto& e : d.errors) r.errors.push_back(std::move(e));
  }
  // Inverse operations undo in reverse order.
  std::reverse(r.inverse.begin(), r.inverse.end());
  if (!r.ok()) r.inverse.clear();
  return r;
}

}  // namespace sov::cmd
