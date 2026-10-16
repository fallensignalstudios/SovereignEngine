#include "sov/world/scene_diff.h"

#include <format>

namespace sov::scene {
namespace {

void DiffJson(const std::string& path, const Json& a, const Json& b, std::vector<FieldChange>& out) {
  if (a.is_object() && b.is_object()) {
    for (auto it = a.begin(); it != a.end(); ++it) {
      const std::string p = path.empty() ? it.key() : path + "." + it.key();
      auto bit = b.find(it.key());
      DiffJson(p, it.value(), bit == b.end() ? Json(nullptr) : *bit, out);
    }
    for (auto it = b.begin(); it != b.end(); ++it) {
      if (a.find(it.key()) == a.end()) {
        out.push_back({path.empty() ? it.key() : path + "." + it.key(), Json(nullptr), it.value()});
      }
    }
    return;
  }
  if (a != b) out.push_back({path, a, b});
}

void FlattenAll(const std::string& path, const Json& j, bool as_after, std::vector<FieldChange>& out) {
  if (j.is_object()) {
    for (auto it = j.begin(); it != j.end(); ++it) {
      FlattenAll(path.empty() ? it.key() : path + "." + it.key(), it.value(), as_after, out);
    }
    return;
  }
  if (as_after) {
    out.push_back({path, Json(nullptr), j});
  } else {
    out.push_back({path, j, Json(nullptr)});
  }
}

Json EntityBody(const EntityData& e) {
  Json j = Json::object();
  j["name"] = e.name;
  j["components"] = ComponentsToJson(e);
  return j;
}

Json SceneLevel(const SceneDocument& d) {
  Json j = SceneToJson(d);
  j.erase("entities");
  j.erase("revision");
  j.erase("format");
  return j;
}

Json ChangesToJson(const std::vector<FieldChange>& changes) {
  Json arr = Json::array();
  for (const auto& c : changes) arr.push_back(Json{{"path", c.path}, {"before", c.before}, {"after", c.after}});
  return arr;
}

Json EntityChangesToJson(const std::vector<EntityChange>& list) {
  Json arr = Json::array();
  for (const auto& e : list) {
    arr.push_back(Json{{"entity_id", e.id.ToString()}, {"name", e.name}, {"changes", ChangesToJson(e.changes)}});
  }
  return arr;
}

}  // namespace

SceneDiff DiffScenes(const SceneDocument& before, const SceneDocument& after) {
  SceneDiff diff;
  diff.revision_before = before.revision;
  diff.revision_after = after.revision;
  DiffJson("", SceneLevel(before), SceneLevel(after), diff.scene);

  // Both entity lists are sorted by id: merge walk.
  std::size_t i = 0, j = 0;
  while (i < before.entities.size() || j < after.entities.size()) {
    const EntityData* a = i < before.entities.size() ? &before.entities[i] : nullptr;
    const EntityData* b = j < after.entities.size() ? &after.entities[j] : nullptr;
    if (a && (!b || a->id < b->id)) {
      EntityChange c{a->id, a->name, {}};
      FlattenAll("", EntityBody(*a), false, c.changes);
      diff.deleted.push_back(std::move(c));
      ++i;
    } else if (b && (!a || b->id < a->id)) {
      EntityChange c{b->id, b->name, {}};
      FlattenAll("", EntityBody(*b), true, c.changes);
      diff.created.push_back(std::move(c));
      ++j;
    } else {
      if (!(*a == *b)) {
        EntityChange c{b->id, b->name, {}};
        DiffJson("", EntityBody(*a), EntityBody(*b), c.changes);
        diff.modified.push_back(std::move(c));
      }
      ++i;
      ++j;
    }
  }
  return diff;
}

Json DiffToJson(const SceneDiff& d) {
  Json j = Json::object();
  j["revision_before"] = d.revision_before;
  j["revision_after"] = d.revision_after;
  j["entities_created"] = EntityChangesToJson(d.created);
  j["entities_deleted"] = EntityChangesToJson(d.deleted);
  j["entities_modified"] = EntityChangesToJson(d.modified);
  j["scene_fields"] = ChangesToJson(d.scene);
  j["summary"] = std::format("{} created, {} deleted, {} modified, {} scene field(s) changed", d.created.size(),
                             d.deleted.size(), d.modified.size(), d.scene.size());
  return j;
}

std::string DiffToText(const SceneDiff& d) {
  std::string out = std::format("revision {} -> {}\n", d.revision_before, d.revision_after);
  if (d.empty()) {
    out += "  (no content changes)\n";
    return out;
  }
  for (const auto& c : d.scene) {
    out += std::format("  ~ scene.{}: {} -> {}\n", c.path, CompactJson(c.before), CompactJson(c.after));
  }
  for (const auto& e : d.created) out += std::format("  + {} \"{}\"\n", e.id.ToString(), e.name);
  for (const auto& e : d.deleted) out += std::format("  - {} \"{}\"\n", e.id.ToString(), e.name);
  for (const auto& e : d.modified) {
    out += std::format("  ~ {} \"{}\"\n", e.id.ToString(), e.name);
    for (const auto& c : e.changes) {
      out += std::format("      {}: {} -> {}\n", c.path, CompactJson(c.before), CompactJson(c.after));
    }
  }
  return out;
}

}  // namespace sov::scene
