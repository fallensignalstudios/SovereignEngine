#include "sov/games/sandbox.h"

#include <algorithm>
#include <cmath>

#include "sov/core/log.h"

namespace sov::games {
namespace {

class SandboxGame final : public runtime::GameModule {
 public:
  explicit SandboxGame(SandboxConfig c) : config_(c) {}

  std::string_view name() const override { return "sandbox"; }

  void Begin(World& world) override {
    camera_ = world.ActiveCamera();
    Vec3 origin{0, 1.5, 0};
    Vec3 forward{0, 0, -1};
    if (const auto m = world.GetWorldMatrix(camera_)) {
      origin = GetTranslation(*m);
      forward = Normalize(TransformDirection(*m, {0, 0, -1}));
      // Recover yaw/pitch from the camera's local rotation for mouse-look continuity.
      if (const auto* t = world.GetTransform(camera_)) {
        const EulerDegrees e = EulerDegreesFromQuat(t->rotation);
        yaw_ = e.yaw;
        pitch_ = e.pitch;
      }
    }
    scene::EntityData probe;
    probe.id = GenerateUuid();  // runtime-only identity; never written to an authoring document
    probe.name = "Play Probe";
    probe.transform.position = origin + forward * config_.probe_distance_m;
    probe.transform.scale = {0.6, 0.6, 0.6};
    probe.primitive_mesh = scene::PrimitiveMeshComponent{scene::PrimitiveShape::Box};
    probe.material = scene::MaterialComponent{{0.1, 0.1, 0.1}, 0.4, 0.0, {0.25, 0.85, 1.0}, 3.0};
    probe_base_ = probe.transform.position;
    if (auto h = world.CreateEntity(probe)) {
      probe_ = *h;
    } else {
      LogWarn("sandbox", "could not spawn play probe: {}", h.error().message);
    }
  }

  void Tick(World& world, const runtime::ActionState& in, const runtime::TickInfo& tick) override {
    const double dt = tick.dt_seconds;
    const double t = static_cast<double>(tick.tick + 1) * dt;
    if (world.IsValid(probe_)) {
      const Quat spin = Quat::FromAxisAngle({0, 1, 0}, DegToRad(config_.probe_spin_deg_per_s * t));
      const Vec3 bob = probe_base_ + Vec3{0, 0.25 * std::sin(t * 2.0), 0};
      world.SetLocalTransform(probe_, bob, spin, {0.6, 0.6, 0.6});
    }
    if (!world.IsValid(camera_)) return;
    const auto* ct = world.GetTransform(camera_);
    if (!ct) return;
    yaw_ += in.look_yaw_deg;
    pitch_ = std::clamp(pitch_ + in.look_pitch_deg, -89.0, 89.0);
    const Quat rot = QuatFromEulerDegrees({pitch_, yaw_, 0});
    const double speed = config_.move_speed_mps * (in.sprint ? config_.sprint_multiplier : 1.0);
    const Vec3 fwd = Rotate(rot, {0, 0, -1});
    const Vec3 right = Rotate(rot, {1, 0, 0});
    const Vec3 move = fwd * in.move_forward + right * in.move_right + Vec3{0, 1, 0} * in.move_up;
    world.SetLocalTransform(camera_, ct->position + move * (speed * dt), rot, ct->scale);
  }

 private:
  SandboxConfig config_;
  EntityHandle camera_{};
  EntityHandle probe_{};
  Vec3 probe_base_{};
  double yaw_ = 0;
  double pitch_ = 0;
};

}  // namespace

std::unique_ptr<runtime::GameModule> MakeSandboxGame(SandboxConfig config) {
  return std::make_unique<SandboxGame>(config);
}

}  // namespace sov::games
