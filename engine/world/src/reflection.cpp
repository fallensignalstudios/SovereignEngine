#include "sov/world/reflection.h"

#include <array>
#include <cmath>
#include <format>

namespace sov::scene {
namespace {

constexpr double kQuatTolerance = 1e-3;

// --- transform -------------------------------------------------------------------------------
const FieldDescriptor kTransformFields[] = {
    {.key = "parent",
     .type = FieldType::EntityRef,
     .label = "Parent",
     .description = "Parent entity; local values are relative to it. Null for a scene root.",
     .access = FieldAccess::TransformSet,
     .get = [](const EntityData& e) -> FieldValue { return e.transform.parent; },
     .set = [](EntityData& e, const FieldValue& v) { e.transform.parent = std::get<std::optional<Uuid>>(v); },
     .default_value = []() -> FieldValue { return std::optional<Uuid>{}; }},
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
    {.key = "rotation",
     .type = FieldType::Quat,
     .label = "Rotation",
     .description = "Local rotation as a unit quaternion [x, y, z, w]; the editor displays Euler degrees.",
     .access = FieldAccess::TransformSet,
     .get = [](const EntityData& e) -> FieldValue { return e.transform.rotation; },
     .set = [](EntityData& e, const FieldValue& v) { e.transform.rotation = std::get<Quat>(v); },
     .default_value = []() -> FieldValue { return Quat{}; }},
    {.key = "scale",
     .type = FieldType::Vec3,
     .label = "Scale",
     .description = "Local scale factors; strictly positive in G1.",
     .access = FieldAccess::TransformSet,
     .min = 1e-4,
     .max = 1e4,
     .get = [](const EntityData& e) -> FieldValue { return e.transform.scale; },
     .set = [](EntityData& e, const FieldValue& v) { e.transform.scale = std::get<Vec3>(v); },
     .default_value = []() -> FieldValue { return Vec3{1, 1, 1}; }},
};

// --- primitive_mesh --------------------------------------------------------------------------
constexpr std::string_view kShapeNames[] = {"box", "plane", "cylinder"};
const FieldDescriptor kPrimitiveFields[] = {
    {.key = "shape",
     .type = FieldType::Enum,
     .label = "Shape",
     .description = "Built-in unit primitive. box: 1 m cube; plane: 1 m square facing +Y; cylinder: r 0.5 m, h 1 m along Y.",
     .access = FieldAccess::CreateOnly,
     .enum_values = kShapeNames,
     .get = [](const EntityData& e) -> FieldValue { return std::string(ToString(e.primitive_mesh->shape)); },
     .set = [](EntityData& e, const FieldValue& v) { e.primitive_mesh->shape = *ParsePrimitiveShape(std::get<std::string>(v)); },
     .default_value = []() -> FieldValue { return std::string("box"); }},
};

// --- material --------------------------------------------------------------------------------
const FieldDescriptor kMaterialFields[] = {
    {.key = "base_color",
     .type = FieldType::Color,
     .label = "Base color",
     .description = "Linear RGB reflectance.",
     .access = FieldAccess::MaterialSet,
     .min = 0.0,
     .max = 1.0,
     .get = [](const EntityData& e) -> FieldValue { return e.material->base_color; },
     .set = [](EntityData& e, const FieldValue& v) { e.material->base_color = std::get<Vec3>(v); },
     .default_value = []() -> FieldValue { return Vec3{0.8, 0.8, 0.8}; }},
    {.key = "roughness",
     .type = FieldType::Number,
     .label = "Roughness",
     .description = "Specular spread; 0.02 is mirror-like, 1 is fully diffuse.",
     .access = FieldAccess::MaterialSet,
     .min = 0.02,
     .max = 1.0,
     .get = [](const EntityData& e) -> FieldValue { return e.material->roughness; },
     .set = [](EntityData& e, const FieldValue& v) { e.material->roughness = std::get<double>(v); },
     .default_value = []() -> FieldValue { return 0.6; }},
    {.key = "metallic",
     .type = FieldType::Number,
     .label = "Metallic",
     .description = "0 = dielectric, 1 = metal (specular tinted by base color).",
     .access = FieldAccess::MaterialSet,
     .min = 0.0,
     .max = 1.0,
     .get = [](const EntityData& e) -> FieldValue { return e.material->metallic; },
     .set = [](EntityData& e, const FieldValue& v) { e.material->metallic = std::get<double>(v); },
     .default_value = []() -> FieldValue { return 0.0; }},
    {.key = "emissive",
     .type = FieldType::Color,
     .label = "Emissive color",
     .description = "Linear RGB emission color. Emission is not illumination in G1.",
     .access = FieldAccess::MaterialSet,
     .min = 0.0,
     .max = 1.0,
     .get = [](const EntityData& e) -> FieldValue { return e.material->emissive; },
     .set = [](EntityData& e, const FieldValue& v) { e.material->emissive = std::get<Vec3>(v); },
     .default_value = []() -> FieldValue { return Vec3{0, 0, 0}; }},
    {.key = "emissive_strength",
     .type = FieldType::Number,
     .label = "Emissive strength",
     .description = "Multiplier on the emissive color (linear, pre-exposure).",
     .access = FieldAccess::MaterialSet,
     .min = 0.0,
     .max = 100.0,
     .get = [](const EntityData& e) -> FieldValue { return e.material->emissive_strength; },
     .set = [](EntityData& e, const FieldValue& v) { e.material->emissive_strength = std::get<double>(v); },
     .default_value = []() -> FieldValue { return 0.0; }},
};

// --- camera ----------------------------------------------------------------------------------
const FieldDescriptor kCameraFields[] = {
    {.key = "vertical_fov_degrees",
     .type = FieldType::Number,
     .label = "Vertical FOV",
     .description = "Vertical field of view in degrees.",
     .access = FieldAccess::CreateOnly,
     .min = 10.0,
     .max = 120.0,
     .get = [](const EntityData& e) -> FieldValue { return e.camera->vertical_fov_degrees; },
     .set = [](EntityData& e, const FieldValue& v) { e.camera->vertical_fov_degrees = std::get<double>(v); },
     .default_value = []() -> FieldValue { return 60.0; }},
    {.key = "near_clip",
     .type = FieldType::Number,
     .label = "Near clip",
     .description = "Near plane distance in meters (maps to depth 1, reversed-Z).",
     .access = FieldAccess::CreateOnly,
     .min = 0.01,
     .max = 10.0,
     .get = [](const EntityData& e) -> FieldValue { return e.camera->near_clip; },
     .set = [](EntityData& e, const FieldValue& v) { e.camera->near_clip = std::get<double>(v); },
     .default_value = []() -> FieldValue { return 0.1; }},
    {.key = "far_clip",
     .type = FieldType::Number,
     .label = "Far clip",
     .description = "Far plane distance in meters (maps to depth 0); must exceed near_clip.",
     .access = FieldAccess::CreateOnly,
     .min = 1.0,
     .max = 1e6,
     .get = [](const EntityData& e) -> FieldValue { return e.camera->far_clip; },
     .set = [](EntityData& e, const FieldValue& v) { e.camera->far_clip = std::get<double>(v); },
     .default_value = []() -> FieldValue { return 1000.0; }},
};

// --- directional_light -----------------------------------------------------------------------
const FieldDescriptor kLightFields[] = {
    {.key = "color",
     .type = FieldType::Color,
     .label = "Color",
     .description = "Linear RGB light color.",
     .access = FieldAccess::CreateOnly,
     .min = 0.0,
     .max = 1.0,
     .get = [](const EntityData& e) -> FieldValue { return e.directional_light->color; },
     .set = [](EntityData& e, const FieldValue& v) { e.directional_light->color = std::get<Vec3>(v); },
     .default_value = []() -> FieldValue { return Vec3{1, 1, 1}; }},
    {.key = "intensity",
     .type = FieldType::Number,
     .label = "Intensity",
     .description = "Linear illuminance multiplier.",
     .access = FieldAccess::CreateOnly,
     .min = 0.0,
     .max = 100.0,
     .get = [](const EntityData& e) -> FieldValue { return e.directional_light->intensity; },
     .set = [](EntityData& e, const FieldValue& v) { e.directional_light->intensity = std::get<double>(v); },
     .default_value = []() -> FieldValue { return 3.0; }},
};

const ComponentDescriptor kComponents[] = {
    {.key = "transform",
     .label = "Transform",
     .description = "Local placement relative to the parent (meters, unit quaternion, scale).",
     .required = true,
     .fields = kTransformFields,
     .has = [](const EntityData&) { return true; },
     .add_default = [](EntityData& e) { e.transform = {}; }},
    {.key = "primitive_mesh",
     .label = "Primitive mesh",
     .description = "Built-in unit shape rendered with the entity's material.",
     .fields = kPrimitiveFields,
     .has = [](const EntityData& e) { return e.primitive_mesh.has_value(); },
     .add_default = [](EntityData& e) { e.primitive_mesh.emplace(); }},
    {.key = "material",
     .label = "Material",
     .description = "Simple lit material constants (linear color).",
     .fields = kMaterialFields,
     .has = [](const EntityData& e) { return e.material.has_value(); },
     .add_default = [](EntityData& e) { e.material.emplace(); }},
    {.key = "camera",
     .label = "Camera",
     .description = "Perspective camera looking along local -Z (reversed-Z depth).",
     .fields = kCameraFields,
     .has = [](const EntityData& e) { return e.camera.has_value(); },
     .add_default = [](EntityData& e) { e.camera.emplace(); }},
    {.key = "directional_light",
     .label = "Directional light",
     .description = "Infinitely distant light travelling along local -Z. At most one per scene in G1.",
     .fields = kLightFields,
     .has = [](const EntityData& e) { return e.directional_light.has_value(); },
     .add_default = [](EntityData& e) { e.directional_light.emplace(); }},
};

Result<double> ParseNumber(const Json& j, const std::string& path) {
  if (!j.is_number()) return MakeError(ErrorCode::SchemaError, "expected a number", path);
  const double v = j.get<double>();
  if (!std::isfinite(v)) return MakeError(ErrorCode::InvalidValue, "number must be finite", path);
  return v;
}

Result<void> CheckRange(const FieldDescriptor& f, double v, const std::string& path) {
  if (v < f.min || v > f.max) {
    return MakeError(ErrorCode::InvalidValue, std::format("{} is outside [{}, {}]", v, f.min, f.max), path);
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
      auto a = ParseFixedArray(j, 3, path);
      if (!a) return a.take_error();
      for (std::size_t i = 0; i < 3; ++i) {
        if (auto r = CheckRange(f, (*a)[i], JsonPointerAppend(path, i)); !r) return r.take_error();
      }
      return FieldValue{Vec3{(*a)[0], (*a)[1], (*a)[2]}};
    }
    case FieldType::Quat: {
      auto a = ParseFixedArray(j, 4, path);
      if (!a) return a.take_error();
      const Quat q{(*a)[0], (*a)[1], (*a)[2], (*a)[3]};
      const double len = Length(q);
      if (std::abs(len - 1.0) > kQuatTolerance) {
        return MakeError(ErrorCode::InvalidValue,
                         std::format("rotation must be a unit quaternion [x, y, z, w]; length is {}", len), path);
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
