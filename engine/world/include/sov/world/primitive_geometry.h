#pragma once

#include <cstdint>
#include <vector>

#include "sov/world/scene_document.h"

namespace sov {

// CPU definition of the built-in unit primitives (see PrimitiveMeshComponent for sizes).
// Triangles are wound counter-clockwise when viewed from outside (front face), matching
// VK_FRONT_FACE_COUNTER_CLOCKWISE with the engine projection.
struct PrimitiveVertex {
  float position[3];
  float normal[3];
  float uv[2];
};
static_assert(sizeof(PrimitiveVertex) == 32);

struct PrimitiveGeometry {
  std::vector<PrimitiveVertex> vertices;
  std::vector<std::uint32_t> indices;
};

inline constexpr int kCylinderSegments = 32;

PrimitiveGeometry BuildPrimitive(scene::PrimitiveShape shape);
std::uint32_t PrimitiveTriangleCount(scene::PrimitiveShape shape);

}  // namespace sov
