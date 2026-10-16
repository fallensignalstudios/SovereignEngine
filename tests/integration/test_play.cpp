// Play sessions: isolated runtime worlds built from the authoring document.
#include <filesystem>

#include "sov/core/fs.h"
#include "sov/editor/editor_document.h"
#include "sov/games/sandbox.h"
#include "sov/world/scene_document.h"
#include "sov_test.h"

using namespace sov;
using namespace sov::editor;

namespace {

std::filesystem::path StageScene(sov::test::Context& ctx) {
  const auto dst = ctx.TempDir() / "first_light.scene.json";
  std::filesystem::copy_file(ctx.SourceDir() / "content/authoring/first_light.scene.json", dst);
  return dst;
}

std::string Read(const std::filesystem::path& p) { return *fs::ReadFile(p, 1 << 26); }

runtime::ActionState FlyForwardAndTurn() {
  runtime::ActionState in;
  in.move_forward = 1.0;
  in.move_right = 0.5;
  in.look_yaw_deg = 0.5;
  return in;
}

}  // namespace

SOV_TEST(play, modifies_only_the_runtime_world) {
  const auto path = StageScene(ctx);
  const std::string file_before = Read(path);
  const auto mtime_before = std::filesystem::last_write_time(path);
  auto opened = EditorDocument::Open(path);
  SOV_REQUIRE_OK(opened);
  EditorDocument& doc = **opened;
  const std::string content_before = scene::ContentHash(doc.document());
  const auto revision_before = doc.revision();
  const std::size_t authored_entities = doc.document().entities.size();
  const Uuid camera_id = *doc.document().active_camera;
  const Vec3 authored_camera = doc.document().Find(camera_id)->transform.position;

  SOV_REQUIRE_OK(doc.BeginPlay(games::MakeSandboxGame()));
  runtime::Simulation& sim = *doc.play();
  SOV_CHECK(sim.world().kind() == WorldKind::Play);
  SOV_CHECK(sim.world().id() != doc.authoring_world().id());
  sim.StepTicks(120, FlyForwardAndTurn());  // two seconds at 60 Hz

  // The play world changed: a spawned probe, and the camera flew.
  SOV_CHECK_EQ(sim.world().EntityCount(), authored_entities + 1);
  const auto cam = sim.world().GetTransform(sim.world().Find(camera_id));
  SOV_REQUIRE(cam);
  SOV_CHECK(Length(cam->position - authored_camera) > 5.0);  // ~4 m/s * 2 s plus strafe
  // Authoring state did not: document, authoring world, revision, file.
  SOV_CHECK_EQ(scene::ContentHash(doc.document()), content_before);
  SOV_CHECK_EQ(doc.revision(), revision_before);
  SOV_CHECK(!doc.dirty());
  SOV_CHECK_EQ(doc.authoring_world().EntityCount(), authored_entities);
  SOV_CHECK(doc.authoring_world().GetTransform(doc.authoring_world().Find(camera_id))->position == authored_camera);
  // Play-world handles never resolve in the authoring world.
  const EntityHandle play_cam = sim.world().Find(camera_id);
  SOV_CHECK(!doc.authoring_world().IsValid(play_cam));

  doc.EndPlay();
  SOV_CHECK(!doc.playing());
  SOV_CHECK_EQ(scene::ContentHash(doc.document()), content_before);
  // Saving after play writes nothing (no pending authored change).
  SOV_REQUIRE_OK(doc.Save());
  SOV_CHECK_EQ(Read(path), file_before);
  SOV_CHECK(std::filesystem::last_write_time(path) == mtime_before);
}

SOV_TEST(play, fifty_play_stop_cycles_return_to_baseline) {
  const auto path = StageScene(ctx);
  auto opened = EditorDocument::Open(path);
  SOV_REQUIRE_OK(opened);
  EditorDocument& doc = **opened;
  const std::string content = scene::ContentHash(doc.document());
  const std::size_t worlds_baseline = World::LiveWorldCount();
  for (int i = 0; i < 50; ++i) {
    SOV_REQUIRE_OK(doc.BeginPlay(games::MakeSandboxGame()));
    SOV_CHECK_EQ(World::LiveWorldCount(), worlds_baseline + 1);
    doc.play()->StepTicks(30, FlyForwardAndTurn());
    doc.EndPlay();
    SOV_CHECK_EQ(World::LiveWorldCount(), worlds_baseline);
  }
  SOV_CHECK_EQ(scene::ContentHash(doc.document()), content);
  SOV_CHECK_EQ(doc.authoring_world().EntityCount(), doc.document().entities.size());
  SOV_CHECK(!doc.dirty());
}

SOV_TEST(play, repeated_runs_of_same_build_are_repeatable) {
  const auto path = StageScene(ctx);
  auto loaded = scene::LoadSceneFile(path);
  SOV_REQUIRE_OK(loaded);
  Vec3 first{};
  for (int run = 0; run < 2; ++run) {
    auto sim = runtime::Simulation::Create(loaded->document, WorldKind::Runtime, games::MakeSandboxGame());
    SOV_REQUIRE_OK(sim);
    (*sim)->StepTicks(90, FlyForwardAndTurn());
    const auto m = (*sim)->world().GetWorldMatrix((*sim)->world().ActiveCamera());
    SOV_REQUIRE(m.has_value());
    if (run == 0) {
      first = GetTranslation(*m);
    } else {
      SOV_CHECK(GetTranslation(*m) == first);  // bitwise: same build, same inputs, same tick count
    }
  }
}

SOV_TEST(play, fixed_step_clock_caps_catch_up) {
  runtime::FixedStepClock clock(60.0, 4);
  SOV_CHECK_EQ(clock.Advance(1.0 / 60.0 + 1e-9), 1);
  SOV_CHECK_EQ(clock.Advance(0.004), 0);  // accumulates below one step
  SOV_CHECK_EQ(clock.Advance(1.0), 4);    // a one-second hitch runs at most 4 steps...
  SOV_CHECK(clock.dropped_seconds() > 0.9);  // ...and the rest is dropped, not replayed later
  SOV_CHECK(clock.backlog_seconds() < clock.step_seconds());
  SOV_CHECK_EQ(clock.capped_advances(), std::uint64_t{1});
}
