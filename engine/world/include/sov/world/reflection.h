#pragma once

#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

#include "sov/core/json.h"
#include "sov/core/math.h"
#include "sov/core/result.h"
#include "sov/core/uuid.h"
#include "sov/world/scene_document.h"

// Reflection descriptors for components. One table drives:
//   - strict document parsing and serialization (scene_document.cpp)
//   - command allowlists (which operation may write which field)
//   - editor inspector widgets (labels, ranges, read-only state)
// No raw offsets or addresses are exposed; access is through typed accessor functions.
namespace sov::scene {

enum class FieldType {
  Number,     // finite double within [min, max] (exclusive_min: strictly greater than min)
  Vec3,       // 3 finite doubles, each within [min, max]
  Color,      // linear RGB, each within [min, max]
  Color4,     // linear RGBA, each within [min, max]
  Quat,       // [x, y, z, w], each in [-1, 1], |q|^2 within SceneLimits::kQuatNormTolerance of 1
  Enum,       // one of enum_values
  EntityRef,  // null or canonical UUID string
};

// Which forward-edit operation may write the field after creation.
enum class FieldAccess {
  CreateOnly,    // set by entity.create; read-only afterwards in G1
  TransformSet,  // transform.set
  MaterialSet,   // material.set
};

using FieldValue = std::variant<double, Vec3, Vec4, Quat, std::string, std::optional<Uuid>>;

struct FieldDescriptor {
  std::string_view key;
  FieldType type;
  std::string_view label;
  std::string_view description;
  FieldAccess access;
  double min = -std::numeric_limits<double>::infinity();
  double max = std::numeric_limits<double>::infinity();
  bool exclusive_min = false;
  std::span<const std::string_view> enum_values = {};
  bool persisted = true;
  bool editor_only = false;
  FieldValue (*get)(const EntityData&) = nullptr;
  void (*set)(EntityData&, const FieldValue&) = nullptr;
  FieldValue (*default_value)() = nullptr;
};

struct ComponentDescriptor {
  std::string_view key;
  std::string_view label;
  std::string_view description;
  int version = 1;
  bool required = false;  // present on every entity
  std::span<const FieldDescriptor> fields;
  bool (*has)(const EntityData&) = nullptr;
  void (*add_default)(EntityData&) = nullptr;  // emplace with field defaults
};

// Components in canonical serialization order.
std::span<const ComponentDescriptor> Components();
const ComponentDescriptor* FindComponent(std::string_view key);
const FieldDescriptor* FindField(const ComponentDescriptor& component, std::string_view key);
const ComponentDescriptor& TransformDescriptor();
const ComponentDescriptor& MaterialDescriptor();

// Parses and range-checks one field value. `path` is the JSON pointer used in errors.
Result<FieldValue> ParseFieldValue(const FieldDescriptor& field, const Json& json, const std::string& path);
Json FieldValueToJson(const FieldDescriptor& field, const FieldValue& value);
bool FieldValuesEqual(const FieldValue& a, const FieldValue& b);

std::string_view ToString(FieldAccess access);

}  // namespace sov::scene
