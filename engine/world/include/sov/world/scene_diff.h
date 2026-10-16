#pragma once

#include <string>
#include <vector>

#include "sov/core/json.h"
#include "sov/world/scene_document.h"

namespace sov::scene {

struct FieldChange {
  std::string path;  // e.g. "components.Transform.position", "name", "background_linear_rgb"
  Json before;       // null when the field/component did not exist
  Json after;        // null when the field/component was removed
};

struct EntityChange {
  Uuid id;
  std::string name;  // name after the change (before, for deletions)
  std::vector<FieldChange> changes;
};

// Structural difference between two documents of the same scene. Revision is reported
// separately and never counts as content.
struct SceneDiff {
  std::uint64_t revision_before = 0;
  std::uint64_t revision_after = 0;
  std::vector<EntityChange> created;   // `changes` lists every initial field
  std::vector<EntityChange> deleted;   // `changes` lists every removed field
  std::vector<EntityChange> modified;
  std::vector<FieldChange> scene;      // scene-level fields (name, active_camera_id, background_linear_rgb)

  bool empty() const { return created.empty() && deleted.empty() && modified.empty() && scene.empty(); }
};

SceneDiff DiffScenes(const SceneDocument& before, const SceneDocument& after);
Json DiffToJson(const SceneDiff& diff);
std::string DiffToText(const SceneDiff& diff);

}  // namespace sov::scene
