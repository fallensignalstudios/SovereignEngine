#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "sov/core/math.h"
#include "sov/core/result.h"
#include "sov/core/uuid.h"
#include "sov/world/render_snapshot.h"
#include "sov/world/scene_document.h"

namespace sov {

enum class WorldKind { Authoring, Play, Runtime };
std::string_view ToString(WorldKind kind);

// Generation-checked runtime handle. `world` identifies the owning World instance (never 0 for a
// live handle), so a handle from the authoring world cannot address an entity in a play world
// that happens to reuse the same slot. Never persisted; never derived from a Uuid.
struct EntityHandle {
  std::uint32_t world = 0;
  std::uint32_t value = 0;  // slot index + generation (opaque)
  bool IsNull() const { return world == 0; }
  bool operator==(const EntityHandle&) const = default;
};

// In-memory ECS world built from a validated scene document. EnTT storage is private to this
// class; callers see only EntityHandle and engine component types.
// Threading: single owner thread mutates; no internal locking (TDD §7.3).
class World {
 public:
  explicit World(WorldKind kind);
  ~World();
  World(const World&) = delete;
  World& operator=(const World&) = delete;

  // Builds a world containing every entity of `doc`, parents linked, transforms propagated.
  static Result<std::unique_ptr<World>> FromDocument(const scene::SceneDocument& doc, WorldKind kind);

  std::uint32_t id() const { return id_; }
  WorldKind kind() const { return kind_; }

  EntityHandle Find(const Uuid& id) const;
  bool IsValid(EntityHandle h) const;
  std::optional<Uuid> GetUuid(EntityHandle h) const;
  std::size_t EntityCount() const;

  // Adds an entity; fails on duplicate id or missing parent. Runtime systems use this to spawn
  // transient entities in play/runtime worlds (they never reach the authoring document).
  Result<EntityHandle> CreateEntity(const scene::EntityData& data);
  // Refuses (MissingDependency) while children still reference the entity: no hidden cascade.
  Result<void> DestroyEntity(EntityHandle h);

  const scene::TransformComponent* GetTransform(EntityHandle h) const;
  bool SetLocalTransform(EntityHandle h, const Vec3& position, const Quat& rotation, const Vec3& scale);
  const scene::MaterialComponent* GetMaterial(EntityHandle h) const;
  bool SetMaterial(EntityHandle h, const scene::MaterialComponent& material);
  const std::string* GetName(EntityHandle h) const;

  // World matrix as of the last UpdateTransforms().
  std::optional<Mat4> GetWorldMatrix(EntityHandle h) const;

  // Recomputes world matrices in parent-before-child order. Called automatically by FromDocument;
  // call after local transform edits before reading world matrices or extracting a snapshot.
  void UpdateTransforms();

  // Order used by UpdateTransforms (each parent precedes its children). Exposed for tests.
  std::vector<EntityHandle> PropagationOrder() const;

  EntityHandle ActiveCamera() const { return active_camera_; }
  void SetActiveCamera(EntityHandle h) { active_camera_ = h; }
  const scene::Environment& environment() const { return environment_; }

  // Immutable render-facing copy of the current state (TDD §10.9). Fills pick_table with
  // index -> Uuid for this snapshot only.
  RenderSnapshot ExtractRenderSnapshot() const;

  void ForEachEntity(const std::function<void(EntityHandle)>& fn) const;

  // Number of World instances currently alive in this process (leak checks in play/stop loops).
  static std::size_t LiveWorldCount();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  std::uint32_t id_;
  WorldKind kind_;
  EntityHandle active_camera_{};
  scene::Environment environment_{};
};

}  // namespace sov
