#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "sov/core/math.h"
#include "sov/core/uuid.h"
#include "sov/world/scene_document.h"

namespace sov {

// Plain-data snapshot published by simulation and consumed by the renderer. The renderer may cache
// GPU state derived from it but never owns or writes gameplay/authoring state.
struct RenderInstance {
  Mat4 model;
  Mat4 normal_matrix;
  scene::PrimitiveShape shape = scene::PrimitiveShape::Box;
  scene::MaterialComponent material;
  std::uint32_t pick_index = 0;  // 1-based index into RenderSnapshot::pick_table; 0 = not pickable
};

struct RenderCamera {
  Mat4 world;  // camera-to-world; the camera looks along its local -Z
  double vertical_fov_radians = 1.0;
  double near_clip = 0.1;
  double far_clip = 1000.0;
};

struct RenderSnapshot {
  std::vector<RenderInstance> instances;
  std::vector<Uuid> pick_table;  // pick_table[i - 1] is the entity for pick index i (this frame only)
  std::optional<RenderCamera> active_camera;
  Vec3 background_linear_rgb{0.02, 0.02, 0.025};  // scene radiance behind all geometry
  std::uint64_t triangle_count = 0;

  // Entity UUID for a pick index read back from the ID attachment, if any.
  std::optional<Uuid> ResolvePick(std::uint32_t pick_index) const {
    if (pick_index == 0 || pick_index > pick_table.size()) return std::nullopt;
    return pick_table[pick_index - 1];
  }
  std::uint32_t PickIndexOf(const Uuid& id) const {
    for (std::size_t i = 0; i < pick_table.size(); ++i) {
      if (pick_table[i] == id) return static_cast<std::uint32_t>(i + 1);
    }
    return 0;
  }
};

}  // namespace sov
