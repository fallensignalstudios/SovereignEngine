#include "sov/world/primitive_geometry.h"

#include <cmath>

#include "sov/core/math.h"

namespace sov {
namespace {

void AddVertex(PrimitiveGeometry& g, const Vec3& p, const Vec3& n, double u, double v) {
  g.vertices.push_back({{static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z)},
                        {static_cast<float>(n.x), static_cast<float>(n.y), static_cast<float>(n.z)},
                        {static_cast<float>(u), static_cast<float>(v)}});
}

// Quad facing `n` with in-plane axes u, v where u x v == n. Corners at n/2 +- u/2 +- v/2 scaled by `half`.
void AddFace(PrimitiveGeometry& g, const Vec3& n, const Vec3& u, const Vec3& v, double offset) {
  const auto base = static_cast<std::uint32_t>(g.vertices.size());
  const Vec3 c = n * offset;
  AddVertex(g, c - u * 0.5 - v * 0.5, n, 0, 1);
  AddVertex(g, c + u * 0.5 - v * 0.5, n, 1, 1);
  AddVertex(g, c + u * 0.5 + v * 0.5, n, 1, 0);
  AddVertex(g, c - u * 0.5 + v * 0.5, n, 0, 0);
  for (std::uint32_t i : {0u, 1u, 2u, 0u, 2u, 3u}) g.indices.push_back(base + i);
}

}  // namespace

PrimitiveGeometry BuildPrimitive(scene::PrimitiveShape shape) {
  PrimitiveGeometry g;
  switch (shape) {
    case scene::PrimitiveShape::Box:
      AddFace(g, {1, 0, 0}, {0, 0, -1}, {0, 1, 0}, 0.5);
      AddFace(g, {-1, 0, 0}, {0, 0, 1}, {0, 1, 0}, 0.5);
      AddFace(g, {0, 1, 0}, {1, 0, 0}, {0, 0, -1}, 0.5);
      AddFace(g, {0, -1, 0}, {1, 0, 0}, {0, 0, 1}, 0.5);
      AddFace(g, {0, 0, 1}, {1, 0, 0}, {0, 1, 0}, 0.5);
      AddFace(g, {0, 0, -1}, {-1, 0, 0}, {0, 1, 0}, 0.5);
      break;
    case scene::PrimitiveShape::Plane:
      AddFace(g, {0, 1, 0}, {1, 0, 0}, {0, 0, -1}, 0.0);
      break;
    case scene::PrimitiveShape::Cylinder: {
      constexpr int n = kCylinderSegments;
      const double r = 0.5;
      // Side: angle increases counter-clockwise seen from +Y; point = (r cos a, y, -r sin a).
      for (int i = 0; i <= n; ++i) {
        const double a = 2.0 * kPi * i / n;
        const Vec3 normal{std::cos(a), 0, -std::sin(a)};
        AddVertex(g, {r * normal.x, -0.5, r * normal.z}, normal, static_cast<double>(i) / n, 1);
        AddVertex(g, {r * normal.x, 0.5, r * normal.z}, normal, static_cast<double>(i) / n, 0);
      }
      for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(n); ++i) {
        const std::uint32_t b0 = 2 * i, t0 = 2 * i + 1, b1 = 2 * i + 2, t1 = 2 * i + 3;
        for (std::uint32_t k : {b0, b1, t1, b0, t1, t0}) g.indices.push_back(k);
      }
      // Caps.
      for (int cap = 0; cap < 2; ++cap) {
        const double y = cap == 0 ? 0.5 : -0.5;
        const Vec3 normal{0, cap == 0 ? 1.0 : -1.0, 0};
        const auto center = static_cast<std::uint32_t>(g.vertices.size());
        AddVertex(g, {0, y, 0}, normal, 0.5, 0.5);
        for (int i = 0; i < n; ++i) {
          const double a = 2.0 * kPi * i / n;
          AddVertex(g, {r * std::cos(a), y, -r * std::sin(a)}, normal, 0.5 + 0.5 * std::cos(a), 0.5 + 0.5 * std::sin(a));
        }
        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(n); ++i) {
          const std::uint32_t p0 = center + 1 + i;
          const std::uint32_t p1 = center + 1 + (i + 1) % static_cast<std::uint32_t>(n);
          if (cap == 0) {
            for (std::uint32_t k : {center, p0, p1}) g.indices.push_back(k);
          } else {
            for (std::uint32_t k : {center, p1, p0}) g.indices.push_back(k);
          }
        }
      }
      break;
    }
  }
  return g;
}

std::uint32_t PrimitiveTriangleCount(scene::PrimitiveShape shape) {
  switch (shape) {
    case scene::PrimitiveShape::Box: return 12;
    case scene::PrimitiveShape::Plane: return 2;
    case scene::PrimitiveShape::Cylinder: return static_cast<std::uint32_t>(kCylinderSegments * 4);
  }
  return 0;
}

}  // namespace sov
