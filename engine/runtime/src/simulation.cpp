#include "sov/runtime/simulation.h"

#include <algorithm>

namespace sov::runtime {

FixedStepClock::FixedStepClock(double hz, int max_steps_per_advance)
    : step_(1.0 / hz), max_steps_(max_steps_per_advance) {}

int FixedStepClock::Advance(double real_seconds) {
  accumulator_ += std::max(0.0, real_seconds);
  int steps = static_cast<int>(accumulator_ / step_);
  if (steps > max_steps_) {
    dropped_ += (steps - max_steps_) * step_;
    accumulator_ -= (steps - max_steps_) * step_;
    steps = max_steps_;
    ++capped_;
  }
  accumulator_ -= steps * step_;
  return steps;
}

Result<std::unique_ptr<Simulation>> Simulation::Create(const scene::SceneDocument& doc, WorldKind kind,
                                                       std::unique_ptr<GameModule> game) {
  auto world = World::FromDocument(doc, kind);
  if (!world) return world.take_error();
  std::unique_ptr<Simulation> sim(new Simulation());
  sim->world_ = std::move(world).value();
  sim->game_ = std::move(game);
  if (sim->game_) sim->game_->Begin(*sim->world_);
  sim->world_->UpdateTransforms();
  return sim;
}

Simulation::~Simulation() {
  if (game_ && world_) game_->End(*world_);
}

void Simulation::StepOnce(const ActionState& input) {
  if (game_) game_->Tick(*world_, input, TickInfo{tick_, clock_.step_seconds()});
  world_->UpdateTransforms();
  ++tick_;
}

int Simulation::Advance(double real_seconds, const ActionState& input) {
  const int steps = clock_.Advance(real_seconds);
  for (int i = 0; i < steps; ++i) {
    // Look deltas are consumed by the first step so they are not multiplied by catch-up steps.
    StepOnce(i == 0 ? input : ActionState{input.move_forward, input.move_right, input.move_up, 0, 0, input.sprint});
  }
  return steps;
}

void Simulation::StepTicks(std::uint64_t n, const ActionState& input) {
  for (std::uint64_t i = 0; i < n; ++i) StepOnce(input);
}

}  // namespace sov::runtime
