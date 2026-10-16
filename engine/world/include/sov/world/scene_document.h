#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "sov/core/json.h"
#include "sov/core/math.h"
#include "sov/core/result.h"
#include "sov/core/uuid.h"

// Authoring scene document (schema "sovereign.scene", version 1).
// Human-authored JSON is the canonical source; this is its typed in-memory form.
// Spatial values are local to the parent (right-handed, +Y up, meters); colors are linear RGB.
namespace sov::scene {

enum class PrimitiveShape { Box, Plane, Cylinder };

struct TransformComponent {
  std::optional<Uuid> parent;  // local space is the parent's space; absent = scene root
  Vec3 position{0, 0, 0};
  Quat rotation{0, 0, 0, 1};
  Vec3 scale{1, 1, 1};
  bool operator==(const TransformComponent&) const = default;
};

// Unit-sized built-in shapes; Transform.scale sets dimensions.
//   box: 1 m cube centred on the origin
//   plane: 1 m square in XZ at y = 0, single-sided, facing +Y
//   cylinder: radius 0.5 m, height 1 m along Y, centred on the origin
struct PrimitiveMeshComponent {
  PrimitiveShape shape = PrimitiveShape::Box;
  bool operator==(const PrimitiveMeshComponent&) const = default;
};

struct MaterialComponent {
  Vec3 base_color{0.8, 0.8, 0.8};  // linear RGB reflectance
  double roughness = 0.6;
  double metallic = 0.0;
  Vec3 emissive{0, 0, 0};          // linear RGB; radiance = emissive * emissive_strength
  double emissive_strength = 0.0;
  bool operator==(const MaterialComponent&) const = default;
};

// Looks along local -Z with +Y up.
struct CameraComponent {
  double vertical_fov_degrees = 60.0;
  double near_clip = 0.1;
  double far_clip = 1000.0;
  bool operator==(const CameraComponent&) const = default;
};

// Light travels along the entity's local -Z axis.
struct DirectionalLightComponent {
  Vec3 color{1, 1, 1};  // linear RGB
  double intensity = 3.0;
  bool operator==(const DirectionalLightComponent&) const = default;
};

// Name is an always-present entity field (a label, never a key). Transform is required.
struct EntityData {
  Uuid id;
  std::string name;
  TransformComponent transform;
  std::optional<PrimitiveMeshComponent> primitive_mesh;
  std::optional<MaterialComponent> material;
  std::optional<CameraComponent> camera;
  std::optional<DirectionalLightComponent> directional_light;
  bool operator==(const EntityData&) const = default;
};

struct Environment {
  Vec3 ambient_color{0.35, 0.35, 0.4};  // linear RGB
  double ambient_intensity = 0.25;
  Vec3 background_color{0.02, 0.02, 0.025};
  Vec3 fog_color{0.05, 0.04, 0.04};
  double fog_density = 0.0;  // per meter, exponential
  double exposure = 1.0;     // linear multiplier before tone mapping
  bool operator==(const Environment&) const = default;
};

struct SceneDocument {
  int schema_version = 1;
  Uuid scene_id;
  std::string name;
  std::uint64_t revision = 0;               // authoring revision; monotonic across commits
  std::array<std::int64_t, 3> region_cell{0, 0, 0};  // reserved region identity; must be 0,0,0 in G1
  std::optional<Uuid> active_camera;
  Environment environment;
  std::vector<EntityData> entities;         // invariant: ascending by id (canonical order)

  const EntityData* Find(const Uuid& id) const;
  EntityData* Find(const Uuid& id);
  // Inserts keeping canonical order; returns false if the id already exists.
  bool Insert(EntityData entity);
  bool Erase(const Uuid& id);

  bool operator==(const SceneDocument&) const = default;
};

// Limits applied when reading documents and staging edits.
struct SceneLimits {
  static constexpr std::size_t kMaxFileBytes = 32u * 1024u * 1024u;
  static constexpr std::size_t kMaxEntities = 10000;
  static constexpr std::size_t kMaxNameBytes = 128;
  static constexpr int kMaxHierarchyDepth = 64;
  static constexpr double kMaxCoordinate = 100000.0;  // G1 uses local float coordinates only
};

struct Diagnostics {
  std::vector<Error> errors;
  std::vector<std::string> warnings;
  bool ok() const { return errors.empty(); }
  void Add(Error e);
  // First error, or an Internal error if empty (for callers that need a single Error).
  Error First() const;
};

std::string_view ToString(PrimitiveShape shape);
std::optional<PrimitiveShape> ParsePrimitiveShape(std::string_view text);

// Validates a name label: 1..kMaxNameBytes bytes, no control characters.
std::optional<std::string> ValidateName(std::string_view name);

// Parse a JSON value as a scene document with full strict validation (unknown fields,
// unknown components, unknown schema versions, value ranges, structure). Errors carry JSON pointers.
Result<SceneDocument> ParseSceneDocument(const Json& json, Diagnostics* diagnostics = nullptr);
Result<SceneDocument> ParseSceneText(std::string_view text, Diagnostics* diagnostics = nullptr);

// Structural checks shared by readers and the command stager: duplicate ids, missing/cyclic
// parents, hierarchy depth, active camera reference, light count, entity count.
void ValidateStructure(const SceneDocument& doc, Diagnostics& diagnostics);

// Strict parse of an "environment" object (same rules as inside a scene document).
Result<Environment> ParseEnvironment(const Json& json);

// Canonical serialized form (schema field order, entities ascending by id).
Json SceneToJson(const SceneDocument& doc);
std::string SerializeScene(const SceneDocument& doc);
Json EntityToJson(const EntityData& entity);
Json ComponentsToJson(const EntityData& entity);

// Hash of the authored content excluding the revision counter (used to prove that undo restores
// content while revisions keep increasing).
std::string ContentHash(const SceneDocument& doc);

// Capture/report label that identifies the exact authored state without exposing host paths:
//   "First Light (first_light.scene.json, revision 1, content sha256 0123abcd...)"
std::string ProvenanceLabel(const SceneDocument& doc, const std::filesystem::path& file);

struct LoadedScene {
  SceneDocument document;
  std::string file_bytes;
  std::string file_sha256;
  std::vector<std::string> warnings;
};
Result<LoadedScene> LoadSceneFile(const std::filesystem::path& path);

// Depth of an entity in the hierarchy (roots are 0); -1 if a cycle or missing parent is found.
int HierarchyDepth(const SceneDocument& doc, const Uuid& id);

// New minimal valid scene with one camera entity set as active camera.
SceneDocument MakeNewScene(const Uuid& scene_id, std::string name, const Uuid& camera_id);

}  // namespace sov::scene
