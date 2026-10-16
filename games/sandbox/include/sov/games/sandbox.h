#pragma once

#include <memory>

#include "sov/runtime/simulation.h"

// Original sandbox game module for G1 play sessions and the standalone runtime:
//   - flies the scene's active camera from named input actions (meters/second, degrees)
//   - spawns one transient "Play Probe" entity in front of the camera and animates it, so a play
//     session visibly changes its runtime world without touching the authoring document
namespace sov::games {

struct SandboxConfig {
  double move_speed_mps = 4.0;
  double sprint_multiplier = 3.0;
  double probe_distance_m = 6.0;
  double probe_spin_deg_per_s = 90.0;
};

std::unique_ptr<runtime::GameModule> MakeSandboxGame(SandboxConfig config = {});

}  // namespace sov::games
