// Shared shader declarations. Contract (docs/architecture/conventions.md, "Shader interface"):
//   - compiled by DXC with -spirv -fspv-target-env=vulkan1.3 -Zpc (column-major packing)
//   - CPU matrices are column-major; shaders multiply column vectors: mul(M, v)
//   - descriptor set 0; bindings declared explicitly with [[vk::binding(binding, set)]]
//   - CPU-side mirrors: engine/renderer/src/gpu_types.h (static_asserts guard sizes)

struct FrameConstants {
  float4x4 view_proj;
  float4 camera_position;  // xyz world position
  float4 light_direction;  // xyz: direction the light travels (unit); w: 1 if a light exists
  float4 light_radiance;   // rgb: color * intensity
  float4 ambient;          // rgb: ambient color * intensity
  float4 fog;              // rgb: fog color; a: density per meter
};

struct InstanceData {
  float4x4 model;
  float4x4 normal_matrix;        // inverse-transpose of the model's upper 3x3
  float4 base_color_roughness;   // rgb linear base color, a roughness
  float4 emissive;               // rgb: emissive * strength (linear)
  float4 params;                 // x: metallic
  uint4 ids;                     // x: pick index (0 = none)
};
