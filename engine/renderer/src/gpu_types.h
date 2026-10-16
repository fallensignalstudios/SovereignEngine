#pragma once

// CPU mirrors of shader structures (engine/renderer/shaders/common.hlsli). Matrices are float32,
// column-major, matching -Zpc; every member is 16-byte aligned so std140/std430 agree with C++.
#include <cstdint>

#include "sov/core/math.h"

namespace sov::render::gpu {

struct alignas(16) Float4 {
  float x, y, z, w;
};

struct alignas(16) Float4x4 {
  float m[16];  // column-major
};

inline Float4x4 Pack(const Mat4& a) {
  Float4x4 o{};
  for (int i = 0; i < 16; ++i) o.m[i] = static_cast<float>(a.m[static_cast<std::size_t>(i)]);
  return o;
}

inline Float4 Pack(const Vec3& v, double w) {
  return {static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z), static_cast<float>(w)};
}

struct FrameConstants {
  Float4x4 view_proj;
  Float4 camera_position;
  Float4 light_direction;
  Float4 light_radiance;
  Float4 ambient;
  Float4 fog;
};
static_assert(sizeof(FrameConstants) == 144);

struct InstanceData {
  Float4x4 model;
  Float4x4 normal_matrix;
  Float4 base_color_roughness;
  Float4 emissive;
  Float4 params;
  std::uint32_t ids[4];
};
static_assert(sizeof(InstanceData) == 192);

struct TonemapPush {
  Float4 outline_color;
  float exposure;
  std::uint32_t selected_pick;
  std::uint32_t width;
  std::uint32_t height;
};
static_assert(sizeof(TonemapPush) == 32);

struct ProbeInput {
  Float4x4 m;
  Float4 v;
};
static_assert(sizeof(ProbeInput) == 80);

}  // namespace sov::render::gpu
