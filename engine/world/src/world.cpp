#include "sov/world/world.h"

#include <algorithm>
#include <atomic>
#include <format>
#include <unordered_map>

#include <entt/entity/registry.hpp>

#include "sov/world/primitive_geometry.h"

namespace sov {
namespace {

std::atomic<std::uint32_t> g_next_world_id{1};
std::atomic<std::size_t> g_live_worlds{0};

struct IdC {
  Uuid id;
};
struct NameC {
  std::string name;
};
struct ParentLinkC {
  entt::entity parent = entt::null;
};
struct WorldMatrixC {
  Mat4 m;
};

}  // namespace

std::string_view ToString(WorldKind kind) {
  switch (kind) {
    case WorldKind::Authoring: return "authoring";
    case WorldKind::Play: return "play";
    case WorldKind::Runtime: return "runtime";
  }
  return "authoring";
}

struct World::Impl {
  entt::registry reg;
  std::unordered_map<Uuid, entt::entity> by_uuid;
  std::vector<entt::entity> order;
  bool order_dirty = true;

  void RebuildOrder() {
    std::unordered_map<entt::entity, std::vector<entt::entity>> children;
    std::vector<entt::entity> roots;
    for (auto [e, id] : reg.view<IdC>().each()) {
      (void)id;
      const auto* link = reg.try_get<ParentLinkC>(e);
      if (link && link->parent != entt::null) {
        children[link->parent].push_back(e);
      } else {
        roots.push_back(e);
      }
    }
    const auto by_id = [this](entt::entity a, entt::entity b) { return reg.get<IdC>(a).id < reg.get<IdC>(b).id; };
    std::sort(roots.begin(), roots.end(), by_id);
    order.clear();
    order.reserve(reg.view<IdC>().size());
    // Breadth-first from roots: every parent is emitted before any of its children.
    std::vector<entt::entity> frontier = roots;
    while (!frontier.empty()) {
      std::vector<entt::entity> next;
      for (auto e : frontier) {
        order.push_back(e);
        if (auto it = children.find(e); it != children.end()) {
          auto kids = it->second;
          std::sort(kids.begin(), kids.end(), by_id);
          next.insert(next.end(), kids.begin(), kids.end());
        }
      }
      frontier = std::move(next);
    }
    order_dirty = false;
  }
};

World::World(WorldKind kind) : impl_(std::make_unique<Impl>()), id_(g_next_world_id++), kind_(kind) { ++g_live_worlds; }
World::~World() { --g_live_worlds; }

std::size_t World::LiveWorldCount() { return g_live_worlds.load(); }

static entt::entity ToEntt(EntityHandle h) { return static_cast<entt::entity>(h.value); }

EntityHandle World::Find(const Uuid& id) const {
  auto it = impl_->by_uuid.find(id);
  if (it == impl_->by_uuid.end()) return {};
  return {id_, static_cast<std::uint32_t>(entt::to_integral(it->second))};
}

bool World::IsValid(EntityHandle h) const {
  return !h.IsNull() && h.world == id_ && impl_->reg.valid(ToEntt(h));
}

std::optional<Uuid> World::GetUuid(EntityHandle h) const {
  if (!IsValid(h)) return std::nullopt;
  return impl_->reg.get<IdC>(ToEntt(h)).id;
}

std::size_t World::EntityCount() const { return impl_->by_uuid.size(); }

Result<EntityHandle> World::CreateEntity(const scene::EntityData& data) {
  if (impl_->by_uuid.contains(data.id)) {
    return MakeError(ErrorCode::DuplicateEntity, std::format("entity {} already exists in this world", data.id.ToString()));
  }
  entt::entity parent = entt::null;
  if (data.transform.parent) {
    auto it = impl_->by_uuid.find(*data.transform.parent);
    if (it == impl_->by_uuid.end()) {
      return MakeError(ErrorCode::MissingDependency, std::format("parent {} does not exist in this world",
                                                                 data.transform.parent->ToString()));
    }
    parent = it->second;
  }
  auto& reg = impl_->reg;
  const entt::entity e = reg.create();
  reg.emplace<IdC>(e, data.id);
  reg.emplace<NameC>(e, data.name);
  reg.emplace<scene::TransformComponent>(e, data.transform);
  reg.emplace<ParentLinkC>(e, parent);
  reg.emplace<WorldMatrixC>(e);
  if (data.primitive_mesh) reg.emplace<scene::PrimitiveMeshComponent>(e, *data.primitive_mesh);
  if (data.material) reg.emplace<scene::MaterialComponent>(e, *data.material);
  if (data.camera) reg.emplace<scene::CameraComponent>(e, *data.camera);
  impl_->by_uuid.emplace(data.id, e);
  impl_->order_dirty = true;
  return EntityHandle{id_, static_cast<std::uint32_t>(entt::to_integral(e))};
}

Result<void> World::DestroyEntity(EntityHandle h) {
  if (!IsValid(h)) return MakeError(ErrorCode::UnknownEntity, "handle does not refer to a live entity in this world");
  const entt::entity e = ToEntt(h);
  for (auto [child, link] : impl_->reg.view<ParentLinkC>().each()) {
    if (link.parent == e) {
      return MakeError(ErrorCode::MissingDependency, "entity still has children; destroy or re-parent them first");
    }
  }
  if (active_camera_ == h) active_camera_ = {};
  impl_->by_uuid.erase(impl_->reg.get<IdC>(e).id);
  impl_->reg.destroy(e);
  impl_->order_dirty = true;
  return Ok();
}

const scene::TransformComponent* World::GetTransform(EntityHandle h) const {
  return IsValid(h) ? impl_->reg.try_get<scene::TransformComponent>(ToEntt(h)) : nullptr;
}

bool World::SetLocalTransform(EntityHandle h, const Vec3& position, const Quat& rotation, const Vec3& scale) {
  if (!IsValid(h)) return false;
  auto& t = impl_->reg.get<scene::TransformComponent>(ToEntt(h));
  t.position = position;
  t.rotation = rotation;
  t.scale = scale;
  return true;
}

const scene::MaterialComponent* World::GetMaterial(EntityHandle h) const {
  return IsValid(h) ? impl_->reg.try_get<scene::MaterialComponent>(ToEntt(h)) : nullptr;
}

bool World::SetMaterial(EntityHandle h, const scene::MaterialComponent& material) {
  if (!IsValid(h)) return false;
  impl_->reg.emplace_or_replace<scene::MaterialComponent>(ToEntt(h), material);
  return true;
}

const std::string* World::GetName(EntityHandle h) const {
  return IsValid(h) ? &impl_->reg.get<NameC>(ToEntt(h)).name : nullptr;
}

std::optional<Mat4> World::GetWorldMatrix(EntityHandle h) const {
  if (!IsValid(h)) return std::nullopt;
  return impl_->reg.get<WorldMatrixC>(ToEntt(h)).m;
}

void World::UpdateTransforms() {
  if (impl_->order_dirty) impl_->RebuildOrder();
  auto& reg = impl_->reg;
  for (auto e : impl_->order) {
    const auto& t = reg.get<scene::TransformComponent>(e);
    const Mat4 local = Mat4::Compose(t.position, t.rotation, t.scale);
    const auto& link = reg.get<ParentLinkC>(e);
    auto& wm = reg.get<WorldMatrixC>(e);
    wm.m = (link.parent != entt::null) ? reg.get<WorldMatrixC>(link.parent).m * local : local;
  }
}

std::vector<EntityHandle> World::PropagationOrder() const {
  if (impl_->order_dirty) impl_->RebuildOrder();
  std::vector<EntityHandle> out;
  out.reserve(impl_->order.size());
  for (auto e : impl_->order) out.push_back({id_, static_cast<std::uint32_t>(entt::to_integral(e))});
  return out;
}

void World::ForEachEntity(const std::function<void(EntityHandle)>& fn) const {
  for (const auto& h : PropagationOrder()) fn(h);
}

RenderSnapshot World::ExtractRenderSnapshot() const {
  if (impl_->order_dirty) impl_->RebuildOrder();
  const auto& reg = impl_->reg;
  RenderSnapshot snap;
  snap.background_linear_rgb = background_;
  for (auto e : impl_->order) {
    const auto& wm = reg.get<WorldMatrixC>(e).m;
    if (const auto* prim = reg.try_get<scene::PrimitiveMeshComponent>(e)) {
      RenderInstance inst;
      inst.model = wm;
      inst.normal_matrix = NormalMatrix(wm);
      inst.shape = prim->shape;
      if (const auto* mat = reg.try_get<scene::MaterialComponent>(e)) inst.material = *mat;
      snap.pick_table.push_back(reg.get<IdC>(e).id);
      inst.pick_index = static_cast<std::uint32_t>(snap.pick_table.size());
      snap.triangle_count += PrimitiveTriangleCount(prim->shape);
      snap.instances.push_back(inst);
    }
  }
  if (IsValid(active_camera_)) {
    const entt::entity cam = ToEntt(active_camera_);
    if (const auto* c = reg.try_get<scene::CameraComponent>(cam)) {
      RenderCamera rc;
      rc.world = reg.get<WorldMatrixC>(cam).m;
      rc.vertical_fov_radians = DegToRad(c->fov_y_degrees);
      rc.near_clip = c->near_m;
      rc.far_clip = c->far_m;
      snap.active_camera = rc;
    }
  }
  return snap;
}

Result<std::unique_ptr<World>> World::FromDocument(const scene::SceneDocument& doc, WorldKind kind) {
  auto world = std::make_unique<World>(kind);
  world->background_ = doc.background_linear_rgb;
  // Two passes so parents may appear after children in canonical (id) order.
  for (const auto& e : doc.entities) {
    scene::EntityData unparented = e;
    unparented.transform.parent.reset();
    auto created = world->CreateEntity(unparented);
    if (!created) return created.take_error();
  }
  for (const auto& e : doc.entities) {
    if (!e.transform.parent) continue;
    auto child = world->impl_->by_uuid.find(e.id);
    auto parent = world->impl_->by_uuid.find(*e.transform.parent);
    if (parent == world->impl_->by_uuid.end()) {
      return MakeError(ErrorCode::MissingDependency,
                       std::format("entity {} references missing parent {}", e.id.ToString(), e.transform.parent->ToString()));
    }
    world->impl_->reg.get<ParentLinkC>(child->second).parent = parent->second;
    world->impl_->reg.get<scene::TransformComponent>(child->second).parent = e.transform.parent;
  }
  world->impl_->order_dirty = true;
  world->active_camera_ = world->Find(doc.active_camera_id);
  world->UpdateTransforms();
  return world;
}

}  // namespace sov
