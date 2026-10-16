// Diagnostic compute probe: verifies matrix packing and multiplication order in the shader
// independently of CPU memory layout (TDD §6). Both buffer kinds used by the renderer are tested:
// a constant buffer (std140-like) and a structured buffer (std430-like).

struct ProbeInput {
  float4x4 m;
  float4 v;
};

[[vk::binding(0, 0)]] ConstantBuffer<ProbeInput> g_cb;
[[vk::binding(1, 0)]] StructuredBuffer<ProbeInput> g_sb;
[[vk::binding(2, 0)]] RWStructuredBuffer<float4> g_out;

[numthreads(1, 1, 1)]
void CSMain() {
  g_out[0] = mul(g_cb.m, g_cb.v);  // M * v from the constant buffer
  g_out[1] = mul(g_sb[0].m, g_sb[0].v);  // M * v from the structured buffer
  g_out[2] = g_cb.m[0];  // HLSL m[0] is row 0
  g_out[3] = g_sb[0].m[0];
}
