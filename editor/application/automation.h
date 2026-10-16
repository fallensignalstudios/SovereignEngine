#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "editor_app.h"
#include "sov/core/json.h"
#include "sov/core/process.h"

namespace sov::editor_app {

// Unattended editor scripts for evidence capture and lifecycle stress. Steps call the same
// EditorApp / EditorDocument entry points as the GUI widgets (inspector commits, viewport picks,
// menu actions); they do not mutate documents any other way. Each step is logged with its outcome.
//
// Script: {"steps": [ {"do": "<step>", ...}, ... ]}
//   wait_frames {n} | select {name} | pick {name} | move {name, delta:[x,y,z]}
//   set_material {name, field, value} | rename {name, to} | create {shape, name, position}
//   undo | redo | save | reopen | apply_batch {path}
//   play | stop | play_ticks {n} | camera {position:[x,y,z], yaw, pitch}
//   resize_window {w, h} | minimize | restore
//   capture_window {path} | capture_viewport {path}
//   expect {revision?, dirty?, selected?, playing?, document_hash_equals_saved?}
//   cycles {n}  (save, play 15 frames, stop, reopen; checks resources/validation return to baseline)
//   report {path} | quit
class AutomationRunner {
 public:
  static Result<AutomationRunner> Load(const std::filesystem::path& script);

  // Advances the script by one frame. Returns false when finished (check failures()).
  bool Tick(EditorApp& app);
  int failures() const { return failures_; }
  const Json& transcript() const { return transcript_; }

 private:
  bool Step(EditorApp& app, const Json& step);  // true = step complete
  void Record(const Json& step, bool ok, std::string detail);
  const scene::EntityData* Named(EditorApp& app, const Json& step, const char* key = "name");

  std::vector<Json> steps_;
  std::size_t index_ = 0;
  int wait_ = 0;
  int failures_ = 0;
  Json transcript_ = Json::array();
  // per-step state
  bool step_started_ = false;
  std::uint64_t step_frame_ = 0;
  int cycle_ = 0;
  int cycle_phase_ = 0;
  render::ResourceSummary cycle_baseline_{};
  std::uint32_t cycle_validation_baseline_ = 0;
  ProcessMemory cycle_memory_baseline_{};
  ProcessMemory cycle_memory_warm_{};  // after kCycleWarmup cycles (allocator and driver caches populated)
};

}  // namespace sov::editor_app
