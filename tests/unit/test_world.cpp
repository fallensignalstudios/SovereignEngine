#include "scene_fixtures.h"
#include "sov/world/primitive_geometry.h"
#include "sov/world/world.h"
#include "sov_test.h"

using namespace sov;

SOV_TEST(world, builds_from_document_with_parent_before_child_propagation) {
  auto world = World::FromDocument(fixtures::SmallScene(), WorldKind::Authoring);
  SOV_REQUIRE_OK(world);
  World& w = **world;
  SOV_CHECK_EQ(w.EntityCount(), std::size_t{4});

  const EntityHandle parent = w.Find(fixtures::kParentId);
  const EntityHandle child = w.Find(fixtures::kChildId);
  SOV_REQUIRE(w.IsValid(parent) && w.IsValid(child));

  // Order: every parent precedes its children.
  const auto order = w.PropagationOrder();
  const auto pos = [&](EntityHandle h) { return std::find(order.begin(), order.end(), h) - order.begin(); };
  SOV_CHECK(pos(parent) < pos(child));

  // child world = parent world * child local.
  const auto& doc = fixtures::SmallScene();
  const auto& pt = doc.Find(fixtures::kParentId)->transform;
  const auto& ct = doc.Find(fixtures::kChildId)->transform;
  const Mat4 expected = Mat4::Compose(pt.position, pt.rotation, pt.scale) * Mat4::Compose(ct.position, ct.rotation, ct.scale);
  SOV_CHECK(NearlyEqual(*w.GetWorldMatrix(child), expected, 1e-12));
  // Child origin sits 1.5 m above the parent origin (parent has no Y scale or tilt).
  SOV_CHECK(NearlyEqual(GetTranslation(*w.GetWorldMatrix(child)), Vec3{1, 2.0, -2}, 1e-12));
}

SOV_TEST(world, moving_parent_moves_child_after_update) {
  auto world = World::FromDocument(fixtures::SmallScene(), WorldKind::Play);
  SOV_REQUIRE_OK(world);
  World& w = **world;
  const EntityHandle parent = w.Find(fixtures::kParentId);
  const EntityHandle child = w.Find(fixtures::kChildId);
  const auto* t = w.GetTransform(parent);
  SOV_REQUIRE(t);
  SOV_REQUIRE(w.SetLocalTransform(parent, t->position + Vec3{0, 10, 0}, t->rotation, t->scale));
  w.UpdateTransforms();
  SOV_CHECK(NearlyEqual(GetTranslation(*w.GetWorldMatrix(child)), Vec3{1, 12.0, -2}, 1e-12));
}

SOV_TEST(world, handles_are_generation_and_world_checked) {
  auto a = World::FromDocument(fixtures::SmallScene(), WorldKind::Authoring);
  auto b = World::FromDocument(fixtures::SmallScene(), WorldKind::Play);
  SOV_REQUIRE_OK(a);
  SOV_REQUIRE_OK(b);
  const EntityHandle in_a = (*a)->Find(fixtures::kChildId);
  const EntityHandle in_b = (*b)->Find(fixtures::kChildId);
  // Same document, same slot layout, but handles never cross worlds.
  SOV_CHECK_EQ(in_a.value, in_b.value);
  SOV_CHECK(!(*b)->IsValid(in_a));
  SOV_CHECK(!(*a)->IsValid(in_b));

  // Destroy, then recreate: the stale handle stays invalid even if the slot is reused.
  World& w = **a;
  SOV_REQUIRE_OK(w.DestroyEntity(in_a));
  SOV_CHECK(!w.IsValid(in_a));
  scene::EntityData again = *fixtures::SmallScene().Find(fixtures::kChildId);
  auto recreated = w.CreateEntity(again);
  SOV_REQUIRE_OK(recreated);
  SOV_CHECK(w.IsValid(*recreated));
  SOV_CHECK(!w.IsValid(in_a));
  SOV_CHECK(!(*recreated == in_a));
}

SOV_TEST(world, destroy_refuses_while_children_exist) {
  auto world = World::FromDocument(fixtures::SmallScene(), WorldKind::Runtime);
  SOV_REQUIRE_OK(world);
  auto r = (*world)->DestroyEntity((*world)->Find(fixtures::kParentId));
  SOV_REQUIRE(!r.ok());
  SOV_CHECK_EQ(r.error().code, ErrorCode::MissingDependency);
  SOV_CHECK_EQ((*world)->EntityCount(), std::size_t{4});
}

SOV_TEST(world, render_snapshot_contents) {
  auto world = World::FromDocument(fixtures::SmallScene(), WorldKind::Authoring);
  SOV_REQUIRE_OK(world);
  const RenderSnapshot snap = (*world)->ExtractRenderSnapshot();
  SOV_CHECK_EQ(snap.instances.size(), std::size_t{2});  // parent box + child cylinder
  SOV_CHECK_EQ(snap.pick_table.size(), std::size_t{2});
  SOV_REQUIRE(snap.active_camera.has_value());
  // Scene contract v1 has no lights; the snapshot carries the authored background instead.
  SOV_CHECK(snap.background_linear_rgb == fixtures::SmallScene().background_linear_rgb);
  SOV_CHECK_EQ(snap.triangle_count, std::uint64_t{12 + PrimitiveTriangleCount(scene::PrimitiveShape::Cylinder)});
  for (const auto& inst : snap.instances) {
    const auto id = snap.ResolvePick(inst.pick_index);
    SOV_REQUIRE(id.has_value());
    SOV_CHECK_EQ(snap.PickIndexOf(*id), inst.pick_index);
  }
  SOV_CHECK(!snap.ResolvePick(0).has_value());
  SOV_CHECK(!snap.ResolvePick(99).has_value());
}

SOV_TEST(world, primitive_triangles_wound_ccw_from_outside) {
  for (auto shape : {scene::PrimitiveShape::Box, scene::PrimitiveShape::Plane, scene::PrimitiveShape::Cylinder}) {
    const PrimitiveGeometry g = BuildPrimitive(shape);
    SOV_CHECK_EQ(g.indices.size() / 3, std::size_t{PrimitiveTriangleCount(shape)});
    for (std::size_t i = 0; i + 2 < g.indices.size(); i += 3) {
      const auto& a = g.vertices[g.indices[i]];
      const auto& b = g.vertices[g.indices[i + 1]];
      const auto& c = g.vertices[g.indices[i + 2]];
      const Vec3 pa{a.position[0], a.position[1], a.position[2]};
      const Vec3 pb{b.position[0], b.position[1], b.position[2]};
      const Vec3 pc{c.position[0], c.position[1], c.position[2]};
      const Vec3 geometric = Cross(pb - pa, pc - pa);
      const Vec3 n{a.normal[0], a.normal[1], a.normal[2]};
      SOV_CHECK(Dot(geometric, n) > 0);
      for (float coord : {a.position[0], a.position[1], a.position[2]}) SOV_CHECK(coord >= -0.5f && coord <= 0.5f);
    }
  }
}
