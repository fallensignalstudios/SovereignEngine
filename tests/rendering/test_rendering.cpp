// Rendering checks against the real Vulkan backend (offscreen). Every result here carries the
// renderer's evidence class: on a CPU Vulkan implementation they are software-rendered results and
// do not qualify physical-GPU behaviour (run `ctest --preset gpu-qualify` on a GPU host for that).
#include <cmath>
#include <format>
#include <set>
#include <cstring>
#include <algorithm>

#include "sov/core/fs.h"
#include "sov/core/png.h"
#include "sov/renderer/renderer.h"
#include "sov/world/scene_document.h"
#include "sov/world/world.h"
#include "sov_test.h"

using namespace sov;
using namespace sov::render;

namespace {

constexpr std::uint32_t kW = 640, kH = 360;

render::RendererConfig Config() {
  RendererConfig c;
  c.validation = true;
  return c;
}

std::unique_ptr<Renderer>& SharedRenderer(sov::test::Context& ctx) {
  static std::unique_ptr<Renderer> r;
  static bool reported = false;
  if (!r) {
    auto created = Renderer::CreateOffscreen(Config());
    if (!created) {
      ctx.Fail(__FILE__, __LINE__, "renderer creation failed: " + created.error().Describe());
      throw sov::test::AbortTest{};
    }
    r = std::move(created).value();
  }
  if (!reported) {
    reported = true;
    ctx.Note("device: " + r->device().name + " [" + r->device().device_type + "] driver " + r->device().driver);
    ctx.Note("evidence class: " + r->device().evidence_class);
  }
  return r;
}

struct Scene {
  scene::SceneDocument doc;
  std::unique_ptr<World> world;
  RenderSnapshot snap;
  ViewCamera camera;
};

Scene LoadScene(sov::test::Context& ctx, const char* rel) {
  auto loaded = scene::LoadSceneFile(ctx.SourceDir() / rel);
  if (!loaded) {
    ctx.Fail(__FILE__, __LINE__, loaded.error().Describe());
    throw sov::test::AbortTest{};
  }
  Scene s;
  s.doc = loaded->document;
  s.world = std::move(World::FromDocument(s.doc, WorldKind::Runtime).value());
  s.snap = s.world->ExtractRenderSnapshot();
  s.camera = *ViewCamera::FromSnapshot(s.snap);
  return s;
}

const scene::EntityData* ByName(const scene::SceneDocument& d, const std::string& name) {
  for (const auto& e : d.entities) {
    if (e.name == name) return &e;
  }
  return nullptr;
}

// CPU prediction of where a world point lands: pixel (x, y) and reversed-Z depth.
struct Projected {
  std::uint32_t x, y;
  double depth;
};
Projected Project(const ViewCamera& cam, const Vec3& p, std::uint32_t w, std::uint32_t h) {
  const Mat4 vp = cam.Projection(double(w) / double(h)) * cam.ViewMatrix();
  Vec3 ndc;
  ProjectToNdc(vp, p, ndc);
  const Vec2 px = NdcToPixel(ndc, w, h);
  return {static_cast<std::uint32_t>(px.x), static_cast<std::uint32_t>(px.y), ndc.z};
}

Vec3 WorldCenter(const World& w, const Uuid& id) { return GetTranslation(*w.GetWorldMatrix(w.Find(id))); }

void DrawSnapshot(Renderer& r, ViewHandle view, const RenderSnapshot& snap, const ViewCamera& camera,
                  const DrawOptions& opt = {}) {
  if (!r.BeginFrame()) throw std::runtime_error("BeginFrame failed offscreen");
  r.DrawToView(view, snap, camera, opt);
  r.EndFrame();
  r.WaitIdle();
}

void DrawOnce(Renderer& r, ViewHandle view, const Scene& s, const DrawOptions& opt = {}) {
  DrawSnapshot(r, view, s.snap, s.camera, opt);
}

void SaveEvidence(sov::test::Context& ctx, Renderer& r, const ImageRgba8& img, const char* name, const char* scene) {
  const auto path = ctx.TempDir() / name;
  if (WritePng(path, img, CaptureProvenance(r, "offscreen view readback (sovereign-render-tests)", scene))) {
    ctx.Note("capture: " + path.string());
  }
}

}  // namespace

SOV_TEST(rendering, device_startup_with_validation) {
  auto& r = SharedRenderer(ctx);
  const auto& d = r->device();
  SOV_CHECK(!d.name.empty());
  SOV_CHECK(!d.api_version.empty());
  SOV_CHECK_EQ(d.requested_features.size(), std::size_t{4});
  const auto v = r->validation();
  SOV_CHECK_MSG(v.layer_enabled, "VK_LAYER_KHRONOS_validation must be installed for rendering qualification");
  SOV_CHECK(v.synchronization_validation);
  SOV_CHECK_EQ(v.errors, 0u);
  for (const auto& c : d.candidates) ctx.Note("candidate: " + c);
}

SOV_TEST(rendering, shader_matrix_packing_matches_cpu_math) {
  auto& r = SharedRenderer(ctx);
  // Deliberately asymmetric: transposition or row/column mix-ups change the result.
  const Mat4 m = Mat4::Compose({1.5, -2.0, 3.25}, Normalize(Quat{0.3, -0.2, 0.5, 0.8}), {2.0, 0.5, 1.25});
  const Vec4 v{0.7, -1.1, 2.3, 1.0};
  auto probe = r->RunMatrixPackingProbe(m, v);
  SOV_REQUIRE_OK(probe);
  const Vec4 expected = m * v;
  for (int k = 0; k < 2; ++k) {
    const Vec4& got = (*probe)[static_cast<std::size_t>(k)];
    SOV_CHECK_MSG(std::abs(got.x - expected.x) < 1e-4 && std::abs(got.y - expected.y) < 1e-4 &&
                      std::abs(got.z - expected.z) < 1e-4 && std::abs(got.w - expected.w) < 1e-4,
                  std::format("{} buffer: gpu ({}, {}, {}, {}) vs cpu ({}, {}, {}, {})", k == 0 ? "constant" : "structured",
                              got.x, got.y, got.z, got.w, expected.x, expected.y, expected.z, expected.w));
  }
  // HLSL m[0] is row 0 of the CPU matrix.
  for (int k = 2; k < 4; ++k) {
    const Vec4& row = (*probe)[static_cast<std::size_t>(k)];
    SOV_CHECK(std::abs(row.x - m.at(0, 0)) < 1e-5 && std::abs(row.y - m.at(0, 1)) < 1e-5 &&
              std::abs(row.z - m.at(0, 2)) < 1e-5 && std::abs(row.w - m.at(0, 3)) < 1e-5);
  }
  SOV_CHECK_EQ(r->validation().errors, 0u);
}

SOV_TEST(rendering, axis_lab_orientation_depth_culling_hierarchy_and_pick_ids) {
  auto& r = SharedRenderer(ctx);
  Scene s = LoadScene(ctx, "tests/fixtures/axis_lab.scene.json");
  const ViewHandle view = r->CreateView(kW, kH);
  SOV_REQUIRE(view.valid());
  DrawOnce(*r, view, s);
  auto color = r->CaptureView(view);
  auto ids = r->CaptureViewPickIds(view);
  auto depth = r->CaptureViewDepth(view);
  SOV_REQUIRE_OK(color);
  SOV_REQUIRE_OK(ids);
  SOV_REQUIRE_OK(depth);
  SaveEvidence(ctx, *r, *color, "axis_lab.png", "S0 Axis Lab (tests/fixtures/axis_lab.scene.json)");

  const auto id_at = [&](const Projected& p) { return (*ids)[std::size_t(p.y) * kW + p.x]; };
  const auto rgb_at = [&](const Projected& p) { return color->At(p.x, p.y); };
  const auto expect_entity_at = [&](const char* name, const Vec3& world_point) {
    const auto* e = ByName(s.doc, name);
    if (!e) {
      ctx.Fail(__FILE__, __LINE__, std::string("missing fixture entity ") + name);
      return Projected{0, 0, 0};
    }
    const Projected p = Project(s.camera, world_point, kW, kH);
    const std::uint32_t got = id_at(p);
    const auto resolved = s.snap.ResolvePick(got);
    SOV_CHECK_MSG(resolved && *resolved == e->id,
                  std::format("{}: pixel ({}, {}) has pick id {} (expected {})", name, p.x, p.y, got, s.snap.PickIndexOf(e->id)));
    return p;
  };

  // Orientation: +X right of centre, +Y above centre; markers carry their emissive colours.
  const Projected x = expect_entity_at("X Marker (long box)", WorldCenter(*s.world, ByName(s.doc, "X Marker (long box)")->id));
  const Projected y = expect_entity_at("Y Marker (cylinder)", WorldCenter(*s.world, ByName(s.doc, "Y Marker (cylinder)")->id));
  SOV_CHECK(x.x > kW / 2 && std::abs(int(x.y) - int(kH / 2)) < 4);
  SOV_CHECK(y.y < kH / 2 && std::abs(int(y.x) - int(kW / 2)) < 4);
  SOV_CHECK(rgb_at(x)[0] > 150 && rgb_at(x)[1] < 80 && rgb_at(x)[2] < 80);
  SOV_CHECK(rgb_at(y)[1] > 150 && rgb_at(y)[0] < 80 && rgb_at(y)[2] < 80);

  // Depth: the Z marker's front face (z = 2.25) is nearer than the origin cube at screen centre.
  const Vec3 z_front{0, 0, 2.25};
  const Projected zc = expect_entity_at("Z Marker (flat box)", z_front);
  SOV_CHECK(rgb_at(zc)[2] > 150 && rgb_at(zc)[0] < 80);
  const double gpu_depth = (*depth)[std::size_t(zc.y) * kW + zc.x];
  SOV_CHECK_MSG(std::abs(gpu_depth - zc.depth) < 2e-4,
                std::format("reversed-Z depth at centre: gpu {} vs cpu {}", gpu_depth, zc.depth));
  SOV_CHECK(gpu_depth > 0.0 && gpu_depth < 1.0);

  // Depth overlap: the near box occludes the far box where they overlap; the far box shows elsewhere.
  expect_entity_at("Depth Near Box", WorldCenter(*s.world, ByName(s.doc, "Depth Near Box")->id) + Vec3{0, 0, 0.5});
  // (-3.0, -0.95) on the far box's front face (z = -1.5) projects above the near box's front face
  // (near box y in [-2.5, -1.5] at z = 1.5 => normalized y in [-0.294, -0.176]; this point: -0.083).
  expect_entity_at("Depth Far Box", Vec3{-3.0, -0.95, -1.5});

  // Culling: single-sided planes; the one facing away from the camera is not drawn.
  expect_entity_at("Front-Facing Plane", WorldCenter(*s.world, ByName(s.doc, "Front-Facing Plane")->id));
  {
    const auto* back = ByName(s.doc, "Back-Facing Plane");
    const Projected p = Project(s.camera, WorldCenter(*s.world, back->id), kW, kH);
    SOV_CHECK_MSG(id_at(p) != s.snap.PickIndexOf(back->id), "back-facing plane was rasterized");
    SOV_CHECK_EQ(id_at(p), 0u);
  }

  // Hierarchy: child world position = parent T * R * S * child local, and the GPU agrees.
  {
    const auto* child = ByName(s.doc, "Hierarchy Child");
    const Vec3 analytic{-4.0 + 1.8 * std::cos(kPi / 4), 2.0 + 1.8 * std::sin(kPi / 4), 0.0};
    const Vec3 cpu = WorldCenter(*s.world, child->id);
    SOV_CHECK_MSG(NearlyEqual(cpu, analytic, 1e-6), std::format("child at ({}, {}, {})", cpu.x, cpu.y, cpu.z));
    expect_entity_at("Hierarchy Child", cpu + Vec3{0, 0, 0.2});
  }

  // Background: corner pixel has no entity and the cleared background colour (black).
  const Projected corner{2, 2, 0};
  SOV_CHECK_EQ(id_at(corner), 0u);
  SOV_CHECK(rgb_at(corner)[0] < 10 && rgb_at(corner)[1] < 10 && rgb_at(corner)[2] < 10);
  r->DestroyView(view);
  SOV_CHECK_EQ(r->validation().errors, 0u);
}

SOV_TEST(rendering, projection_matches_analytic_fov_and_aspect) {
  // G1-R02 aspect/FOV and G1-R04 "correct aspect/targets after resize", checked against a
  // first-principles pinhole model rather than the engine's own projection helper: a point at
  // view-space (x, y, z) lands at ndc = (x / -z / tan(fov/2) / aspect, y / -z / tan(fov/2)).
  // Axis Lab camera: (0, 0, 10) looking down -Z. X marker: box x in [1, 3], front face z = +0.15.
  // Y marker: cylinder y in [1, 3], radius 0.15 (front at z = +0.15).
  auto& r = SharedRenderer(ctx);
  Scene s = LoadScene(ctx, "tests/fixtures/axis_lab.scene.json");
  const auto* cam = s.doc.Find(s.doc.active_camera_id);
  SOV_REQUIRE(cam && cam->camera);
  const double t = std::tan(DegToRad(cam->camera->fov_y_degrees) / 2.0);
  const double d = 10.0 - 0.15;  // camera to the markers' front faces
  const std::uint32_t x_index = s.snap.PickIndexOf(ByName(s.doc, "X Marker (long box)")->id);
  const std::uint32_t y_index = s.snap.PickIndexOf(ByName(s.doc, "Y Marker (cylinder)")->id);

  const auto check = [&](ViewHandle view, std::uint32_t w, std::uint32_t h, const char* what) {
    DrawOnce(*r, view, s);
    auto ids = r->CaptureViewPickIds(view);
    SOV_REQUIRE_OK(ids);
    SOV_REQUIRE(ids->size() == std::size_t{w} * h);  // targets follow the view size
    const double aspect = double(w) / double(h);
    // Rightmost X-marker pixel on the centre row vs the analytic edge at x = 3.
    const std::uint32_t row = h / 2;
    int right = -1;
    for (std::uint32_t px = 0; px < w; ++px) {
      if ((*ids)[std::size_t(row) * w + px] == x_index) right = int(px);
    }
    const double expect_right = (3.0 / d / t / aspect + 1.0) * 0.5 * w;
    SOV_CHECK_MSG(right >= 0 && std::abs((right + 0.5) - expect_right) <= 1.5,
                  std::format("{} {}x{}: X marker right edge at pixel {} (expected {:.2f})", what, w, h, right, expect_right));
    // Topmost Y-marker pixel on the centre column vs the analytic edge at y = 3.
    const std::uint32_t col = w / 2;
    int top = -1;
    for (std::uint32_t py = h; py-- > 0;) {
      if ((*ids)[std::size_t(py) * w + col] == y_index) top = int(py);
    }
    const double expect_top = (1.0 - 3.0 / d / t) * 0.5 * h;
    SOV_CHECK_MSG(top >= 0 && std::abs((top + 0.5) - expect_top) <= 1.5,
                  std::format("{} {}x{}: Y marker top edge at row {} (expected {:.2f})", what, w, h, top, expect_top));
    ctx.Note(std::format("{} {}x{} (aspect {:.3f}): X right edge {} vs {:.2f}, Y top edge {} vs {:.2f}", what, w, h, aspect,
                         right, expect_right, top, expect_top));
  };
  const ViewHandle wide = r->CreateView(480, 160);
  check(wide, 480, 160, "wide");
  const ViewHandle tall = r->CreateView(240, 360);  // x = 3 lands at ndc 0.79: inside the frame
  check(tall, 240, 360, "tall");
  SOV_REQUIRE(r->ResizeView(wide, 300, 300));  // same view after a resize: aspect and targets follow
  check(wide, 300, 300, "resized");
  r->DestroyView(wide);
  r->DestroyView(tall);
}

SOV_TEST(rendering, request_pick_resolves_entity_with_frame_table) {
  auto& r = SharedRenderer(ctx);
  Scene s = LoadScene(ctx, "tests/fixtures/axis_lab.scene.json");
  const ViewHandle view = r->CreateView(kW, kH);
  const auto* x = ByName(s.doc, "X Marker (long box)");
  const Projected p = Project(s.camera, WorldCenter(*s.world, x->id), kW, kH);
  const std::uint64_t hit = r->RequestPick(view, p.x, p.y);
  const std::uint64_t miss = r->RequestPick(view, 1, 1);
  SOV_CHECK(!r->TakePick(hit).has_value());  // not drawn yet
  DrawOnce(*r, view, s);
  const auto a = r->TakePick(hit);
  const auto b = r->TakePick(miss);
  SOV_REQUIRE(a.has_value() && b.has_value());
  SOV_CHECK(a->entity.has_value() && *a->entity == x->id);
  SOV_CHECK(!b->entity.has_value());
  SOV_CHECK_EQ(b->pick_index, 0u);
  r->DestroyView(view);
}

SOV_TEST(rendering, pick_resolves_with_the_drawn_snapshot_not_a_stale_table) {
  // G1-R03 "no stale frame mapping": a pick is requested while one snapshot is current, then a
  // different snapshot is drawn whose pick indices are permuted (same geometry, reversed order).
  // The texel's index must be resolved with the table of the snapshot actually drawn.
  auto& r = SharedRenderer(ctx);
  Scene s = LoadScene(ctx, "tests/fixtures/axis_lab.scene.json");
  const ViewHandle view = r->CreateView(kW, kH);
  const auto* x = ByName(s.doc, "X Marker (long box)");
  const Projected p = Project(s.camera, WorldCenter(*s.world, x->id), kW, kH);
  DrawOnce(*r, view, s);  // frame A: the original snapshot
  const std::uint32_t index_a = s.snap.PickIndexOf(x->id);

  RenderSnapshot permuted = s.snap;
  std::reverse(permuted.instances.begin(), permuted.instances.end());
  permuted.pick_table.clear();
  for (auto& inst : permuted.instances) {
    const Uuid id = *s.snap.ResolvePick(inst.pick_index);
    permuted.pick_table.push_back(id);
    inst.pick_index = static_cast<std::uint32_t>(permuted.pick_table.size());
  }
  const std::uint32_t index_b = permuted.PickIndexOf(x->id);
  SOV_REQUIRE(index_a != index_b);  // the permutation really changes X's index
  SOV_REQUIRE(s.snap.ResolvePick(index_b) != x->id);  // a stale table would name another entity

  const std::uint64_t ticket = r->RequestPick(view, p.x, p.y);  // requested while A is current...
  DrawSnapshot(*r, view, permuted, s.camera);                   // ...but frame B is what gets drawn
  const auto picked = r->TakePick(ticket);
  SOV_REQUIRE(picked.has_value());
  SOV_CHECK_EQ(picked->pick_index, index_b);
  SOV_REQUIRE(picked->entity.has_value());
  SOV_CHECK(*picked->entity == x->id);
  r->DestroyView(view);
}

SOV_TEST(rendering, selection_outline_changes_only_the_selected_edge) {
  auto& r = SharedRenderer(ctx);
  Scene s = LoadScene(ctx, "tests/fixtures/axis_lab.scene.json");
  const ViewHandle view = r->CreateView(kW, kH);
  DrawOnce(*r, view, s);
  auto plain = r->CaptureView(view);
  const auto* x = ByName(s.doc, "X Marker (long box)");
  DrawOptions opt;
  opt.selected_pick_index = s.snap.PickIndexOf(x->id);
  DrawOnce(*r, view, s, opt);
  auto selected = r->CaptureView(view);
  auto ids = r->CaptureViewPickIds(view);
  SOV_REQUIRE_OK(plain);
  SOV_REQUIRE_OK(selected);
  SOV_REQUIRE_OK(ids);
  std::size_t changed = 0, changed_far = 0;
  for (std::uint32_t py = 0; py < kH; ++py) {
    for (std::uint32_t px = 0; px < kW; ++px) {
      if (std::memcmp(plain->At(px, py), selected->At(px, py), 3) == 0) continue;
      ++changed;
      // Every changed pixel is within 2 px of the selected entity's silhouette.
      bool near_selected = false;
      for (int dy = -2; dy <= 2 && !near_selected; ++dy) {
        for (int dx = -2; dx <= 2 && !near_selected; ++dx) {
          const int qx = std::clamp(int(px) + dx, 0, int(kW) - 1), qy = std::clamp(int(py) + dy, 0, int(kH) - 1);
          near_selected = (*ids)[std::size_t(qy) * kW + qx] == opt.selected_pick_index;
        }
      }
      if (!near_selected) ++changed_far;
    }
  }
  SOV_CHECK(changed > 50);
  SOV_CHECK_EQ(changed_far, std::size_t{0});
  SaveEvidence(ctx, *r, *selected, "axis_lab_selected.png", "S0 Axis Lab, X marker selected");
  r->DestroyView(view);
}

SOV_TEST(rendering, first_light_renders_from_scene_data) {
  auto& r = SharedRenderer(ctx);
  Scene s = LoadScene(ctx, "content/authoring/first_light.scene.json");
  const ViewHandle view = r->CreateView(1280, 720);
  DrawOnce(*r, view, s);
  auto img = r->CaptureView(view);
  auto ids = r->CaptureViewPickIds(view);
  SOV_REQUIRE_OK(img);
  SOV_REQUIRE_OK(ids);
  SaveEvidence(ctx, *r, *img, "first_light.png", "S1 First Light (content/authoring/first_light.scene.json)");
  std::set<std::uint32_t> visible(ids->begin(), ids->end());
  visible.erase(0);
  ctx.Note(std::format("visible entities: {} of {} instances; {} triangles submitted", visible.size(),
                       s.snap.instances.size(), r->last_frame().triangles));
  // The packet's hall: floor, walkway, raised platform, rear wall, six columns, light strip. Every
  // one of its primitives is actually rasterized from the authored camera (none culled or lost).
  SOV_CHECK_EQ(s.snap.instances.size(), std::size_t{11});
  SOV_CHECK_EQ(visible.size(), s.snap.instances.size());
  SOV_CHECK(s.snap.instances.size() <= 100);  // G1 S1 content limits (TDD §21.2)
  SOV_CHECK(r->last_frame().triangles < 100000);
  // Emissive accents produce warm highlights; most of the frame is dark.
  std::size_t bright_warm = 0, dark = 0;
  for (std::size_t i = 0; i < img->pixels.size(); i += 4) {
    const auto* p = &img->pixels[i];
    if (p[0] > 200 && p[1] > 90 && p[2] < 120) ++bright_warm;
    if (p[0] < 50 && p[1] < 50 && p[2] < 50) ++dark;
  }
  SOV_CHECK(bright_warm > 100);
  SOV_CHECK(dark > img->pixels.size() / 4 / 5);
  r->DestroyView(view);
  SOV_CHECK_EQ(r->validation().errors, 0u);
}

SOV_TEST(rendering, view_resize_and_recreate_cycles_return_to_resource_baseline) {
  auto& r = SharedRenderer(ctx);
  Scene s = LoadScene(ctx, "tests/fixtures/axis_lab.scene.json");
  r->WaitIdle();
  const ResourceSummary before = r->resources();
  for (int cycle = 0; cycle < 3; ++cycle) {
    const ViewHandle view = r->CreateView(320, 180);
    for (int i = 0; i < 20; ++i) {
      SOV_REQUIRE(r->ResizeView(view, 200 + 37 * std::uint32_t(i % 7), 120 + 23 * std::uint32_t(i % 5)));
      DrawOnce(*r, view, s);
    }
    r->DestroyView(view);
    SOV_CHECK(!r->IsValid(view));  // stale handle rejected after destroy
  }
  r->WaitIdle();
  const ResourceSummary after = r->resources();
  ctx.Note(std::format("resources before: {} buffers {} images {} allocations; after: {} {} {}", before.buffers,
                       before.images, before.vma_allocations, after.buffers, after.images, after.vma_allocations));
  SOV_CHECK_EQ(after.buffers, before.buffers);
  SOV_CHECK_EQ(after.images, before.images);
  SOV_CHECK_EQ(after.image_views, before.image_views);
  SOV_CHECK_EQ(after.vma_allocations, before.vma_allocations);
  SOV_CHECK_EQ(after.pending_retirements, std::size_t{0});
  SOV_CHECK_EQ(r->validation().errors, 0u);
}

SOV_TEST(rendering, zz_validation_summary_after_suite) {
  // Runs last (suite is sorted by name): reports and asserts the cumulative validation state.
  auto& r = SharedRenderer(ctx);
  const auto v = r->validation();
  ctx.Note(std::format("validation layer {}, synchronization validation {}: {} error(s), {} warning(s)",
                       v.layer_enabled ? "ON" : "OFF", v.synchronization_validation ? "ON" : "OFF", v.errors, v.warnings));
  for (const auto& m : v.messages) ctx.Note(m);
  SOV_CHECK_EQ(v.errors, 0u);
  SOV_CHECK_EQ(v.warnings, 0u);
  r.reset();  // orderly shutdown: views, pipelines, device (leak warnings would be logged)
}
