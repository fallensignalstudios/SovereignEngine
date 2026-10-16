#include "sov/world/reflection.h"

#include <array>
#include <cmath>
#include <format>

// Component and field tables for scene contract v1 (schemas/scene.schema.json). Keys, order,
// ranges and requiredness mirror the schema; scene.schema_files_match_reflection_descriptors keeps
// the two from drifting.
namespace sov::scene {
namespace {

// --- Transform -------------------------------------------------------------------------------
const FieldDescriptor kTransformFields[] = {
    {.key = "position",
     .type = FieldType::Vec3,
     .label = "Position",
     .description = "Local position in meters.",
     .access = FieldAccess::TransformSet,
     .min = -SceneLimits::kMaxCoordinate,
     .max = SceneLimits::kMaxCoordinate,
     .get = [](const EntityData& e) -> FieldValue { return e.transform.position; },
     .set = [](EntityData& e, const FieldValue& v) { e.transform.position = std::get<Vec3>(v); },
     .default_value = []() -> FieldValue { return Vec3{0, 0, 0}; }},
    {.key = "rotation_xyzw",
     .type = FieldType::Quat,
     .label = "Rotation",
     .description = "Local rotation as a normalized quaternion [x, y, z, w]; the editor displays Euler degrees.",
     .access = FieldAccess::TransformSet,
     .min = -1.0,
     .max = 1.0,
     .get = [](const EntityData& e) -> FieldValue { return e.transform.rotation; },
     .set = [](EntityData& e, const FieldValue& v) { e.transform.rotation = std::get<Quat>(v); },
     .default_value = []() -> FieldValue { return Quat{}; }},
    {.key = "scale",
     .type = FieldType::Vec3,
     .label = "Scale",
     .description = "Local scale; with a unit primitive this is its size in meters. Strictly positive in G1.",
     .access = FieldAccess::TransformSet,
     .min = 1e-4,
     .max = 1e4,
     .get = [](const EntityData& e) -> FieldValue { return e.transform.scale; },
     .set = [](EntityData& e, const FieldValue& v) { e.transform.scale = std::get<Vec3>(v); },
     .default_value = []() -> FieldValue { return Vec3{1, 1, 1}; }},
    {.key = "parent_id",
     .type = FieldType::EntityRef,
     .label = "Parent",
     .description = "Parent entity; local values are relative to it. Null for a scene root.",
     .access = FieldAccess::TransformSet,
     .get = [](const EntityData& e) -> FieldValue { return e.transform.parent; },
     .set = [](EntityData& e, const FieldValue& v) { e.transform.parent = std::get<std::optional<Uuid>>(v); },
     .default_value = []() -> FieldValue { return std::optional<Uuid>{}; }},
};

// --- PrimitiveMesh ---------------------------------------------------------------------------
constexpr std::string_view kShapeNames[] = {"box", "plane", "cylinder"};
const FieldDescriptor kPrimitiveFields[] = {
    {.key = "primitive",
     .type = FieldType::Enum,
     .label = "Primitive",
     .description = "Built-in unit primitive. box: side 1 m, centred; plane: 1 m square in XZ facing +Y; "
                    "cylinder: radius 0.5 m, height 1 m along +Y, centred.",
     .access = FieldAccess::CreateOnly,
     .enum_values = kShapeNames,
     .get = [](const EntityData& e) -> FieldValue { return std::string(ToString(e.primitive_mesh->shape)); },
     .set = [](EntityData& e, const FieldValue& v) { e.primitive_mesh->shape = *ParsePrimitiveShape(std::get<std::string>(v)); },
     .default_value = []() -> FieldValue { return std::string("box"); }},
};

// --- MaterialParameters ----------------------------------------------------------------------
const FieldDescriptor kMaterialFields[] = {
    {.key = "base_color_rgba",
     .type = FieldType::Color4,
     .label = "Base color",
     .description = "Linear RGBA. G1 renders opaque: alpha is stored, and values below 1 produce a warning.",
     .access = FieldAccess::MaterialSet,
     .min = 0.0,
     .max = 1.0,
     .get = [](const EntityData& e) -> FieldValue { return e.material->base_color_rgba; },
     .set = [](EntityData& e, const FieldValue& v) { e.material->base_color_rgba = std::get<Vec4>(v); },
     .default_value = []() -> FieldValue { return Vec4{0.8, 0.8, 0.8, 1.0}; }},
    {.key = "emissive_rgb",
     .type = FieldType::Color,
     .label = "Emissive",
     .description = "Linear emitted radiance (HDR, up to 1000). Emission does not light other surfaces in G1.",
     .access = FieldAccess::MaterialSet,
     .min = 0.0,
     .max = 1000.0,
     .get = [](const EntityData& e) -> FieldValue { return e.material->emissive_rgb; },
     .set = [](EntityData& e, const FieldValue& v) { e.material->emissive_rgb = std::get<Vec3>(v); },
     .default_value = []() -> FieldValue { return Vec3{0, 0, 0}; }},
};

// --- Camera ----------------------------------------------------------------------------------
constexpr std::string_view kProjectionNames[] = {"perspective"};
const FieldDescriptor kCameraFields[] = {
    {.key = "projection",
     .type = FieldType::Enum,
     .label = "Projection",
     .description = "Only perspective in contract v1.",
     .access = FieldAccess::CreateOnly,
     .enum_values = kProjectionNames,
     .get = [](const EntityData&) -> FieldValue { return std::string("perspective"); },
     .set = [](EntityData&, const FieldValue&) {},
     .default_value = []() -> FieldValue { return std::string("perspective"); }},
    {.key = "fov_y_degrees",
     .type = FieldType::Number,
     .label = "Vertical FOV",
     .description = "Vertical field of view in degrees.",
     .access = FieldAccess::CreateOnly,
     .min = 1.0,
     .max = 179.0,
     .get = [](const EntityData& e) -> FieldValue { return e.camera->fov_y_degrees; },
     .set = [](EntityData& e, const FieldValue& v) { e.camera->fov_y_degrees = std::get<double>(v); },
     .default_value = []() -> FieldValue { return 60.0; }},
    {.key = "near_m",
     .type = FieldType::Number,
     .label = "Near plane",
     .description = "Near plane distance in meters (maps to depth 1, reversed-Z).",
     .access = FieldAccess::CreateOnly,
     .min = 0.0,
     .max = 10000.0,
     .exclusive_min = true,
     .get = [](const EntityData& e) -> FieldValue { return e.camera->near_m; },
     .set = [](EntityData& e, const FieldValue& v) { e.camera->near_m = std::get<double>(v); },
     .default_value = []() -> FieldValue { return 0.1; }},
    {.key = "far_m",
     .type = FieldType::Number,
     .label = "Far plane",
     .description = "Far plane distance in meters (maps to depth 0); must exceed near_m.",
     .access = FieldAccess::CreateOnly,
     .min = 0.0,
     .max = 1e6,
     .exclusive_min = true,
     .get = [](const EntityData& e) -> FieldValue { return e.camera->far_m; },
     .set = [](EntityData& e, const FieldValue& v) { e.camera->far_m = std::get<double>(v); },
     .default_value = []() -> FieldValue { return 1000.0; }},
};

const ComponentDescriptor kComponents[] = {
    {.key = "Transform",
     .label = "Transform",
     .description = "Local placement relative to the parent (meters, unit quaternion, scale).",
     .required = true,
     .fields = kTransformFields,
     .has = [](const EntityData&) { return true; },
     .add_default = [](EntityData& e) { e.transform = {}; }},
    {.key = "PrimitiveMesh",
     .label = "Primitive mesh",
     .description = "Built-in unit shape; requires MaterialParameters.",
     .fields = kPrimitiveFields,
     .has = [](const EntityData& e) { return e.primitive_mesh.has_value(); },
     .add_default = [](EntityData& e) { e.primitive_mesh.emplace(); }},
    {.key = "MaterialParameters",
     .label = "Material",
     .description = "Simple material constants (linear color); requires PrimitiveMesh.",
     .fields = kMaterialFields,
     .has = [](const EntityData& e) { return e.material.has_value(); },
     .add_default = [](EntityData& e) { e.material.emplace(); }},
    {.key = "Camera",
     .label = "Camera",
     .description = "Perspective camera looking along local -Z (reversed-Z depth).",
     .fields = kCameraFields,
     .has = [](const EntityData& e) { return e.camera.has_value(); },
     .add_default = [](EntityData& e) { e.camera.emplace(); }},
};

Result<double> ParseNumber(const Json& j, const std::string& path) {
  if (!j.is_number()) return MakeError(ErrorCode::SchemaError, "expected a number", path);
  const double v = j.get<double>();
  if (!std::isfinite(v)) return MakeError(ErrorCode::InvalidValue, "number must be finite", path);
  return v;
}

Result<void> CheckRange(const FieldDescriptor& f, double v, const std::string& path) {
  const bool below = f.exclusive_min ? v <= f.min : v < f.min;
  if (below || v > f.max) {
    return MakeError(ErrorCode::InvalidValue,
                     std::format("{} is outside {}{}, {}]", v, f.exclusive_min ? "(" : "[", f.min, f.max), path);
  }
  return Ok();
}

Result<std::array<double, 4>> ParseFixedArray(const Json& j, std::size_t n, const std::string& path) {
  if (!j.is_array() || j.size() != n) {
    return MakeError(ErrorCode::SchemaError, std::format("expected an array of {} numbers", n), path);
  }
  std::array<double, 4> out{};
  for (std::size_t i = 0; i < n; ++i) {
    auto v = ParseNumber(j[i], JsonPointerAppend(path, i));
    if (!v) return v.take_error();
    out[i] = *v;
  }
  return out;
}

Result<std::array<double, 4>> ParseRangedArray(const FieldDescriptor& f, const Json& j, std::size_t n,
                                               const std::string& path) {
  auto a = ParseFixedArray(j, n, path);
  if (!a) return a.take_error();
  for (std::size_t i = 0; i < n; ++i) {
    if (auto r = CheckRange(f, (*a)[i], JsonPointerAppend(path, i)); !r) return r.take_error();
  }
  return *a;
}

}  // namespace

std::span<const ComponentDescriptor> Components() { return kComponents; }

const ComponentDescriptor* FindComponent(std::string_view key) {
  for (const auto& c : kComponents) {
    if (c.key == key) return &c;
  }
  return nullptr;
}

const FieldDescriptor* FindField(const ComponentDescriptor& component, std::string_view key) {
  for (const auto& f : component.fields) {
    if (f.key == key) return &f;
  }
  return nullptr;
}

const ComponentDescriptor& TransformDescriptor() { return kComponents[0]; }
const ComponentDescriptor& MaterialDescriptor() { return kComponents[2]; }

Result<FieldValue> ParseFieldValue(const FieldDescriptor& f, const Json& j, const std::string& path) {
  switch (f.type) {
    case FieldType::Number: {
      auto v = ParseNumber(j, path);
      if (!v) return v.take_error();
      if (auto r = CheckRange(f, *v, path); !r) return r.take_error();
      return FieldValue{*v};
    }
    case FieldType::Vec3:
    case FieldType::Color: {
      auto a = ParseRangedArray(f, j, 3, path);
      if (!a) return a.take_error();
      return FieldValue{Vec3{(*a)[0], (*a)[1], (*a)[2]}};
    }
    case FieldType::Color4: {
      auto a = ParseRangedArray(f, j, 4, path);
      if (!a) return a.take_error();
      return FieldValue{Vec4{(*a)[0], (*a)[1], (*a)[2], (*a)[3]}};
    }
    case FieldType::Quat: {
      auto a = ParseRangedArray(f, j, 4, path);
      if (!a) return a.take_error();
      const Quat q{(*a)[0], (*a)[1], (*a)[2], (*a)[3]};
      const double norm2 = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
      if (std::abs(norm2 - 1.0) > SceneLimits::kQuatNormTolerance) {
        return MakeError(ErrorCode::InvalidValue,
                         std::format("rotation_xyzw must be a normalized quaternion [x, y, z, w]; |q|^2 is {} "
                                     "(tolerance {})",
                                     norm2, SceneLimits::kQuatNormTolerance),
                         path);
      }
      return FieldValue{q};
    }
    case FieldType::Enum: {
      if (!j.is_string()) return MakeError(ErrorCode::SchemaError, "expected a string", path);
      const auto& s = j.get_ref<const std::string&>();
      for (auto allowed : f.enum_values) {
        if (allowed == s) return FieldValue{s};
      }
      std::string list;
      for (auto allowed : f.enum_values) list += (list.empty() ? "" : ", ") + std::string(allowed);
      return MakeError(ErrorCode::InvalidValue, std::format("\"{}\" is not one of: {}", s, list), path);
    }
    case FieldType::EntityRef: {
      if (j.is_null()) return FieldValue{std::optional<Uuid>{}};
      if (!j.is_string()) return MakeError(ErrorCode::SchemaError, "expected null or a UUID string", path);
      auto id = Uuid::Parse(j.get_ref<const std::string&>());
      if (!id) return MakeError(ErrorCode::SchemaError, "not a canonical lowercase UUID", path);
      return FieldValue{std::optional<Uuid>{*id}};
    }
  }
  return MakeError(ErrorCode::Internal, "unhandled field type", path);
}

Json FieldValueToJson(const FieldDescriptor& f, const FieldValue& value) {
  switch (f.type) {
    case FieldType::Number: return Json(std::get<double>(value));
    case FieldType::Vec3:
    case FieldType::Color: {
      const auto& v = std::get<Vec3>(value);
      return Json::array({v.x, v.y, v.z});
    }
    case FieldType::Color4: {
      const auto& v = std::get<Vec4>(value);
      return Json::array({v.x, v.y, v.z, v.w});
    }
    case FieldType::Quat: {
      const auto& q = std::get<Quat>(value);
      return Json::array({q.x, q.y, q.z, q.w});
    }
    case FieldType::Enum: return Json(std::get<std::string>(value));
    case FieldType::EntityRef: {
      const auto& id = std::get<std::optional<Uuid>>(value);
      return id ? Json(id->ToString()) : Json(nullptr);
    }
  }
  return Json(nullptr);
}

bool FieldValuesEqual(const FieldValue& a, const FieldValue& b) { return a == b; }

std::string_view ToString(FieldAccess access) {
  switch (access) {
    case FieldAccess::CreateOnly: return "create_only";
    case FieldAccess::TransformSet: return "transform.set";
    case FieldAccess::MaterialSet: return "material.set";
  }
  return "create_only";
}

}  // namespace sov::scene
