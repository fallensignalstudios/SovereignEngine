# Conventions (math, rendering, data)

Implemented in `engine/core/include/sov/core/math.h`, `engine/renderer`, `engine/world`; locked by
`tests/unit/test_math.cpp`, `tests/unit/test_world.cpp` and `tests/rendering/test_rendering.cpp`.

## Space, units, transforms

| Concern | Convention | Test |
|---|---|---|
| Basis | Right-handed; +X right, +Y up, +Z toward the viewer | `math.right_handed_y_up_basis` |
| Units | Meters; seconds at interfaces; 60 Hz integer ticks for simulation | `play.fixed_step_clock_caps_catch_up` |
| Cameras | Look along local -Z, +Y up | `math.quaternion_serialized_order_and_euler_display` |
| Transform | Column vectors, `v' = M v`, `M = T * R * S`, local to parent | `math.trs_compose_order_is_t_r_s` |
| Hierarchy | World = parent world * local, evaluated parent-before-child (breadth-first from roots) | `world.builds_from_document_with_parent_before_child_propagation` |
| Matrix storage | `Mat4` is column-major: element (r, c) at `m[c*4 + r]`; CPU math is double | same |
| Rotation | Unit quaternion serialized `[x, y, z, w]` (|q| within 1e-3 of 1). The editor shows Euler degrees applied yaw(Y) then pitch(X) then roll(Z): `q = qY * qX * qZ` | `math.quaternion_serialized_order_and_euler_display` |
| Normals | Inverse-transpose of the model's upper 3x3 (`NormalMatrix`) | `math.inverse_and_normal_matrix` |
| Scale | Strictly positive in G1 (`[1e-4, 1e4]`); negative-determinant transforms are a G2 import topic | `scene.value_validation_rejects_bad_numbers` |
| Coordinates | G1 uses local doubles within ±100 km; `region.cell` is reserved and must be `[0,0,0]` | same |

## Projection, depth, culling (Vulkan)

- **Reversed-Z**: depth range [0, 1]; the near plane maps to **1**, the far plane to **0** (a far plane
  of +infinity is supported). Depth is cleared to **0** and compared with **GREATER_OR_EQUAL**.
  `PerspectiveReversedZ` (`math.reversed_z_projection_maps_near_to_one_far_to_zero`).
- **Framebuffer Y**: handled once, by flipping Y **in the projection matrix** (`clip.y = -f * y_view`).
  Viewports always have positive height. We do not also use a negative-height viewport.
  (`math.vulkan_y_flip_puts_up_at_top_of_framebuffer`)
- **Winding**: triangles are counter-clockwise when viewed from their front; the pipeline uses
  `VK_FRONT_FACE_COUNTER_CLOCKWISE` + back-face culling. Proven on the CPU
  (`math.front_face_winding_is_counter_clockwise_on_screen`, `world.primitive_triangles_wound_ccw_from_outside`)
  and on the device (`rendering.axis_lab_...`: the back-facing plane is not rasterized).

## Colour

- CPU material values and lights are **linear RGB**.
- The forward pass writes linear HDR (`R16G16B16A16_SFLOAT`); the tone-map pass applies exposure and
  a fitted ACES curve and writes to an **sRGB** target (`*_SRGB` swapchain or view image). The
  hardware sRGB encode on store is the **single** output encoding step.
- Tool-UI (Dear ImGui) colours are authored in sRGB and converted to linear once at theme setup so
  they display as authored on the sRGB swapchain.
- Captures are the encoded bytes read back from the sRGB image (no further conversion).

## Shader interface

- HLSL (`engine/renderer/shaders`), compiled offline by DXC with
  `-spirv -fspv-target-env=vulkan1.3 -Zpc -HV 2021 -WX -O3` (`cmake/Shaders.cmake`).
- `-Zpc` = column-major packing, matching `Mat4`. Shaders compute `mul(M, v)`; HLSL `m[0]` is row 0.
  Verified on the device independently of CPU layout by the compute probe
  (`rendering.shader_matrix_packing_matches_cpu_math`, constant and structured buffers).
- Descriptor set 0 everywhere; bindings declared explicitly with `[[vk::binding(b, 0)]]`; push
  constants with `[[vk::push_constant]]`. CPU mirrors in `engine/renderer/src/gpu_types.h` carry
  `static_assert`s on their sizes (144 / 192 / 32 / 80 bytes).
- Vertex format: 32 bytes (`float3 position`, `float3 normal`, `float2 uv`); R0 shaders do not read uv.

## Identity and documents

- Persisted IDs: canonical lowercase UUID strings; the nil UUID is invalid. Names are labels.
- Runtime handles (`EntityHandle`) carry the owning world's id plus an EnTT slot+generation value;
  they never cross worlds and are never persisted (`world.handles_are_generation_and_world_checked`).
- Picking uses an `R32_UINT` pick-index target; indices map to UUIDs through the snapshot's
  per-frame `pick_table`. UUIDs are never truncated into pixels.
- Canonical JSON: schema field order, 2-space indent, scalar arrays on one line, shortest
  round-trip doubles, `-0.0` written as `0.0`, entities ascending by UUID, trailing newline.
  Parse then write is a fixed point (`scene.roundtrip_is_byte_identical_and_idempotent`).
