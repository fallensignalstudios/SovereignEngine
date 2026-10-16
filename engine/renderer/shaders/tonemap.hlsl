// Exposure + tone mapping from the linear HDR target to the sRGB output (the hardware sRGB
// encode on store is the single output encoding step). Draws the editor selection outline from
// the pick-ID target so selection is visible without modifying scene materials.

[[vk::binding(0, 0)]] Texture2D<float4> g_hdr;
[[vk::binding(1, 0)]] Texture2D<uint> g_pick;

struct TonemapPush {
  float4 outline_color;   // linear RGB, a = strength
  float exposure;
  uint selected_pick;     // 0 = no selection
  uint width;
  uint height;
};
[[vk::push_constant]] TonemapPush g_push;

struct VSOutput {
  float4 position : SV_Position;
};

VSOutput VSMain(uint id : SV_VertexID) {
  // Full-screen triangle.
  const float2 uv = float2((id << 1) & 2, id & 2);
  VSOutput o;
  o.position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
  return o;
}

// Fitted ACES filmic curve (Narkowicz 2015), applied per channel.
float3 FilmicCurve(float3 x) {
  return saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
}

float4 PSMain(VSOutput input) : SV_Target0 {
  const int2 p = int2(input.position.xy);
  float3 color = FilmicCurve(g_hdr.Load(int3(p, 0)).rgb * g_push.exposure);
  if (g_push.selected_pick != 0) {
    const bool inside = g_pick.Load(int3(p, 0)) == g_push.selected_pick;
    bool edge = false;
    [unroll] for (int dy = -2; dy <= 2; ++dy) {
      [unroll] for (int dx = -2; dx <= 2; ++dx) {
        const int2 q = clamp(p + int2(dx, dy), int2(0, 0), int2(int(g_push.width) - 1, int(g_push.height) - 1));
        edge = edge || ((g_pick.Load(int3(q, 0)) == g_push.selected_pick) != inside);
      }
    }
    if (edge) color = lerp(color, g_push.outline_color.rgb, g_push.outline_color.a);
  }
  return float4(color, 1.0);
}
