#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

#include "sov/core/result.h"
#include "sov/world/scene_document.h"
#include "sov/world/world.h"

namespace sov::runtime {

// Named actions produced by the platform input layer. Gameplay never sees OS key codes.
struct ActionState {
  double move_forward = 0;  // -1..1
  double move_right = 0;    // -1..1
  double move_up = 0;       // -1..1
  double look_yaw_deg = 0;  // accumulated look delta for this tick, degrees (positive = turn left)
  double look_pitch_deg = 0;
  bool sprint = false;
};

struct TickInfo {
  std::uint64_t tick = 0;     // index of the tick being simulated
  double dt_seconds = 0;      // fixed step
};

// Seam between engine orchestration and game rules (TDD §14.6). Game modules mutate only the
// World they are given; they never see authoring documents.
class GameModule {
 public:
  virtual ~GameModule() = default;
  virtual std::string_view name() const = 0;
  virtual void Begin(World& /*world*/) {}
  virtual void Tick(World& world, const ActionState& input, const TickInfo& tick) = 0;
  virtual void End(World& /*world*/) {}
};

// Fixed-rate simulation clock: variable render rate, fixed simulation steps, capped catch-up.
class FixedStepClock {
 public:
  explicit FixedStepClock(double hz = 60.0, int max_steps_per_advance = 4);
  // Accumulates real time and returns the number of fixed steps to run now (<= max steps).
  // Time beyond the cap is dropped and counted (overload policy: drop, never spiral).
  int Advance(double real_seconds);
  double step_seconds() const { return step_; }
  double backlog_seconds() const { return accumulator_; }
  double dropped_seconds() const { return dropped_; }
  std::uint64_t capped_advances() const { return capped_; }

 private:
  double step_;
  int max_steps_;
  double accumulator_ = 0;
  double dropped_ = 0;
  std::uint64_t capped_ = 0;
};

// A runnable world: World + game module + fixed-step clock. Used for editor play sessions
// (WorldKind::Play) and the standalone runtime (WorldKind::Runtime).
class Simulation {
 public:
  static Result<std::unique_ptr<Simulation>> Create(const scene::SceneDocument& doc, WorldKind kind,
                                                    std::unique_ptr<GameModule> game);
  ~Simulation();

  // Interactive: advance by real time; runs 0..max fixed steps. Returns steps run.
  int Advance(double real_seconds, const ActionState& input);
  // Offline/tests: exactly n ticks, no wall clock.
  void StepTicks(std::uint64_t n, const ActionState& input);

  World& world() { return *world_; }
  const World& world() const { return *world_; }
  std::uint64_t tick_count() const { return tick_; }
  const FixedStepClock& clock() const { return clock_; }
  std::string_view game_name() const { return game_ ? game_->name() : std::string_view("none"); }

 private:
  Simulation() = default;
  void StepOnce(const ActionState& input);

  std::unique_ptr<World> world_;
  std::unique_ptr<GameModule> game_;
  FixedStepClock clock_;
  std::uint64_t tick_ = 0;
};

}  // namespace sov::runtime
