#pragma once

#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <SDL3/SDL.h>

#include "imgui_layer.h"
#include "sov/core/log.h"
#include "sov/editor/editor_document.h"
#include "sov/platform/window.h"
#include "sov/renderer/renderer.h"

namespace sov::editor_app {

// Editor-only free camera, independent of the scene's cameras (TDD §11.4: independent focus).
struct EditorCamera {
  Vec3 position{0, 2, 8};
  double yaw_deg = 0;
  double pitch_deg = 0;
  double fov_deg = 60;
  double speed_mps = 6.0;

  Mat4 World() const;
  void SetFromWorld(const Mat4& world);
  void Fly(const runtime::ActionState& in, double dt);
  render::ViewCamera View() const;
};

struct ViewportState {
  render::ViewHandle view;
  std::uint32_t width = 0, height = 0;          // current view size
  std::uint32_t desired_w = 960, desired_h = 540;  // panel content size, applied between frames
  float origin_x = 0, origin_y = 0;              // image position in window pixels (last layout)
  bool hovered = false;
  bool flying = false;                          // RMB held: camera/look capture
  std::optional<std::array<std::uint32_t, 2>> pending_click;  // pixel in view to pick next frame
  std::optional<std::uint64_t> pick_ticket;
  std::optional<render::PickResult> last_pick;
};

struct LogLine {
  LogLevel level;
  std::string text;
};

// Transient editing state of the inspector widgets (preview while dragging, commit on release).
struct InspectorState {
  std::optional<Uuid> entity;
  std::uint64_t loaded_world_generation = 0;
  scene::TransformComponent transform;
  EulerDegrees euler;
  scene::MaterialComponent material;
  std::string name;
  bool editing = false;
};

// Test hook for out-of-process GUI tests (scripts/gui_real_input_check.sh). When SOV_UI_ANCHORS
// names a file, every few frames the editor atomically writes where its widgets are (window pixel
// rectangles) and what state the UI shows, so a test can drive the real widgets with real input
// events and check what is visible. A no-op when disabled; it never changes editor behaviour.
class UiAnchors {
 public:
  void Enable(std::filesystem::path path) { path_ = std::move(path); }
  bool enabled() const { return !path_.empty(); }
  void BeginFrame();
  // Rectangle of the last ImGui item, as [x0, y0, x1, y1] under group/key.
  void Item(std::string_view group, std::string_view key);
  void Rect(std::string_view group, std::string_view key, float x0, float y0, float x1, float y1);
  void Point(std::string_view group, std::string_view key, float x, float y);
  void State(std::string_view key, Json value);
  void Flush(std::uint64_t frame);

 private:
  std::filesystem::path path_;
  Json data_ = Json::object();
};

class EditorApp {
 public:
  EditorApp(platform::Window& window, render::Renderer& renderer, ImGuiLayer& ui,
            std::unique_ptr<editor::EditorDocument> doc);
  ~EditorApp();

  // One editor frame: events must already be fed through HandleEvent. Returns false to quit.
  bool Frame(double dt);
  void HandleEvent(const SDL_Event& e);

  // --- services used by panels and the automation runner ----------------------------------
  editor::EditorDocument& doc() { return *doc_; }
  render::Renderer& renderer() { return renderer_; }
  platform::Window& window() { return window_; }
  ImGuiLayer& ui() { return ui_; }
  EditorCamera& camera() { return camera_; }
  ViewportState& viewport() { return viewport_; }
  InspectorState& inspector() { return inspector_; }
  UiAnchors& anchors() { return anchors_; }
  const std::deque<LogLine>& log() const { return log_; }
  const RenderSnapshot& last_snapshot() const { return last_snapshot_; }

  void StartPlay();
  void StopPlay();
  bool playing() const { return doc_->playing(); }
  void Save();
  Result<void> Reopen();  // close the document (releasing the lock) and open it again from disk
  void RequestQuit() { quit_ = true; }
  void FocusCameraOnSelection();
  void ResetCameraToSceneCamera();
  // Last frame's window capture (with UI) or viewport capture, requested by automation/menus.
  void RequestWindowCapture(std::filesystem::path path);
  void RequestViewportCapture(std::filesystem::path path);

  std::uint64_t frame_number() const { return frame_; }
  bool last_frame_rendered() const { return last_rendered_; }
  std::string status_line() const;

 private:
  void BuildUi();
  void RenderScene();
  void ProcessPicks();
  void ProcessCaptures();
  void RecordAnchorState();

  platform::Window& window_;
  render::Renderer& renderer_;
  ImGuiLayer& ui_;
  std::unique_ptr<editor::EditorDocument> doc_;
  std::filesystem::path scene_path_;
  EditorCamera camera_;
  ViewportState viewport_;
  InspectorState inspector_;
  UiAnchors anchors_;
  platform::ActionMapper input_;
  std::deque<LogLine> log_;
  int log_sink_ = 0;
  RenderSnapshot last_snapshot_;
  bool layout_built_ = false;
  bool quit_ = false;
  bool last_rendered_ = false;
  std::uint64_t frame_ = 0;
  std::optional<std::filesystem::path> window_capture_path_;
  std::optional<std::filesystem::path> viewport_capture_path_;
  bool window_capture_inflight_ = false;
};

// Panels (editor/panels/panels.cpp). Each draws one dockable window.
namespace panels {
void MainMenu(EditorApp& app);
void Hierarchy(EditorApp& app);
void Inspector(EditorApp& app);
void Viewport(EditorApp& app);
void History(EditorApp& app);
void Diagnostics(EditorApp& app);
}  // namespace panels

}  // namespace sov::editor_app
