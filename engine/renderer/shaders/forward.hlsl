// R0 forward pass: simple lit materials into a linear HDR target plus an entity-pick ID target.
#include "common.hlsli"

[[vk::binding(0, 0)]] ConstantBuffer<FrameConstants> g_frame;
[[vk::binding(1, 0)]] StructuredBuffer<InstanceData> g_instances;

struct DrawPush {
  uint instance;
};
[[vk::push_constant]] DrawPush g_push;

struct VSInput {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
};

struct VSOutput {
  float4 position : SV_Position;
  [[vk::location(0)]] float3 world_position : TEXCOORD0;
  [[vk::location(1)]] float3 normal : TEXCOORD1;
  [[vk::location(2)]] nointerpolation uint instance : TEXCOORD2;
};

VSOutput VSMain(VSInput input) {
  const InstanceData inst = g_instances[g_push.instance];
  const float4 world = mul(inst.model, float4(input.position, 1.0));
  VSOutput o;
  o.position = mul(g_frame.view_proj, world);
  o.world_position = world.xyz;
  o.normal = mul((float3x3)inst.normal_matrix, input.normal);
  o.instance = g_push.instance;
  return o;
}

struct PSOutput {
  float4 color : SV_Target0;
  uint pick : SV_Target1;
};

PSOutput PSMain(VSOutput input) {
  const InstanceData inst = g_instances[input.instance];
  const float3 base = inst.base_color_roughness.rgb;
  const float roughness = inst.base_color_roughness.a;
  const float metallic = inst.params.x;

  const float3 n = normalize(input.normal);
  const float3 to_camera = g_frame.camera_position.xyz - input.world_position;
  const float distance_m = length(to_camera);
  const float3 v = to_camera / max(distance_m, 1e-4);
  const float3 specular_color = lerp(float3(0.04, 0.04, 0.04), base, metallic);

  float3 color = float3(0.0, 0.0, 0.0);
  if (g_frame.light_direction.w > 0.5) {
    const float3 l = -g_frame.light_direction.xyz;
    const float n_dot_l = saturate(dot(n, l));
    const float3 h = normalize(l + v);
    const float shininess = exp2(10.0 * (1.0 - roughness) + 1.0);
    // Energy-normalized Blinn-Phong lobe.
    const float spec = pow(saturate(dot(n, h)), shininess) * (shininess + 8.0) / 25.1327412;
    color += g_frame.light_radiance.rgb * n_dot_l * (base * (1.0 - metallic) / 3.14159265 + specular_color * spec);
  }
  // Hemispheric ambient; metals receive a flat stand-in for environment reflection (no IBL in R0).
  const float hemi = 0.65 + 0.35 * n.y;
  color += g_frame.ambient.rgb * hemi * (base * (1.0 - metallic) + specular_color * (0.35 + 0.65 * metallic));
  color += inst.emissive.rgb;

  const float fog = 1.0 - exp(-g_frame.fog.a * distance_m);
  color = lerp(color, g_frame.fog.rgb, fog);

  PSOutput o;
  o.color = float4(color, 1.0);
  o.pick = inst.ids.x;
  return o;
}
