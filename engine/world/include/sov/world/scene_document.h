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

// Authoring scene document: the owner's G1 scene contract v1 (schemas/scene.schema.json,
// urn:sovereign-engine:schema:scene:1, from the First Ignition packet). Human-authored JSON is the
// canonical source; this is its typed in-memory form. Spatial values are local to the parent
// (right-handed, +Y up, meters); colors are linear.
namespace sov::scene {

enum class PrimitiveShape { Box, Plane, Cylinder };

// Wire: Transform { position, rotation_xyzw, scale, parent_id }.
struct TransformComponent {
  std::optional<Uuid> parent;  // parent_id; local space is the parent's space; null = scene root
  Vec3 position{0, 0, 0};
  Quat rotation{0, 0, 0, 1};   // rotation_xyzw
  Vec3 scale{1, 1, 1};
  bool operator==(const TransformComponent&) const = default;
};

// Wire: PrimitiveMesh { primitive }. Unit-sized built-in shapes; Transform.scale sets dimensions.
//   box: 1 m cube centred on the origin
//   plane: 1 m square in XZ at y = 0, single-sided, facing +Y
//   cylinder: radius 0.5 m, height 1 m along Y, centred on the origin
struct PrimitiveMeshComponent {
  PrimitiveShape shape = PrimitiveShape::Box;
  bool operator==(const PrimitiveMeshComponent&) const = default;
};

// Wire: MaterialParameters { base_color_rgba, emissive_rgb }. Simple G1 material: no PBR claims.
// Alpha is stored and round-tripped; the R0 renderer draws opaque (alpha < 1 produces a warning).
struct MaterialComponent {
  Vec4 base_color_rgba{0.8, 0.8, 0.8, 1.0};  // linear RGBA, each in [0, 1]
  Vec3 emissive_rgb{0, 0, 0};                 // linear emitted radiance, each in [0, 1000]
  bool operator==(const MaterialComponent&) const = default;
};

// Wire: Camera { projection: "perspective", fov_y_degrees, near_m, far_m }.
// Looks along local -Z with +Y up.
struct CameraComponent {
  double fov_y_degrees = 60.0;
  double near_m = 0.1;
  double far_m = 1000.0;
  bool operator==(const CameraComponent&) const = default;
};

// Name is an always-present entity field (a label, never a key). Transform is required;
// PrimitiveMesh and MaterialParameters are present together or not at all.
struct EntityData {
  Uuid id;
  std::string name;
  TransformComponent transform;
  std::optional<PrimitiveMeshComponent> primitive_mesh;
  std::optional<MaterialComponent> material;
  std::optional<CameraComponent> camera;
  bool operator==(const EntityData&) const = default;
};

// Scene schema v1 carries no lights or environment: the R0 renderer applies fixed engine
// lighting (docs/architecture/conventions.md) and clears to background_linear_rgb.
struct SceneDocument {
  int schema_version = 1;
  Uuid scene_id;
  std::string name;
  std::uint64_t revision = 0;               // authoring revision; monotonic across commits
  Uuid active_camera_id;                    // must name an entity with a Camera component
  Vec3 background_linear_rgb{0.02, 0.02, 0.025};
  std::vector<EntityData> entities;         // invariant: ascending by id (canonical order)

  const EntityData* Find(const Uuid& id) const;
  EntityData* Find(const Uuid& id);
  // Inserts keeping canonical order; returns false if the id already exists.
  bool Insert(EntityData entity);
  bool Erase(const Uuid& id);

  bool operator==(const SceneDocument&) const = default;
};

// Limits applied when reading documents and staging edits.
// Limits from scene contract v1 where it states them; the rest are engine limits.
struct SceneLimits {
  static constexpr std::size_t kMaxFileBytes = 32u * 1024u * 1024u;
  static constexpr std::size_t kMaxEntities = 10000;   // contract: entities maxItems
  static constexpr std::size_t kMaxNameChars = 160;    // contract: name maxLength (Unicode code points)
  static constexpr int kMaxHierarchyDepth = 64;
  static constexpr double kMaxCoordinate = 1000000.0;  // contract: position in [-1e6, 1e6]
  static constexpr double kQuatNormTolerance = 1e-6;   // |q|^2 within this of 1 (packet checker rule)
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

// Number of Unicode code points in UTF-8 text (JSON Schema maxLength counts these), or nullopt
// if the bytes are not valid UTF-8.
std::optional<std::size_t> Utf8CodePoints(std::string_view text);

// Validates a name label: valid UTF-8, 1..kMaxNameChars code points, no control characters.
std::optional<std::string> ValidateName(std::string_view name);

// Parse a JSON value as a scene document with full strict validation (unknown fields,
// unknown components, unknown schema versions, value ranges, structure). Errors carry JSON pointers.
Result<SceneDocument> ParseSceneDocument(const Json& json, Diagnostics* diagnostics = nullptr);
Result<SceneDocument> ParseSceneText(std::string_view text, Diagnostics* diagnostics = nullptr);

// Structural checks shared by readers and the command stager: duplicate ids, missing/cyclic
// parents, hierarchy depth, active camera reference, entity count (1..kMaxEntities).
void ValidateStructure(const SceneDocument& doc, Diagnostics& diagnostics);

// Strict parse of one entity object ({id, name, components}) with the same rules as inside a
// scene document. Used by entity.create, which carries a complete entity.
Result<EntityData> ParseEntity(const Json& json, const std::string& path);

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
