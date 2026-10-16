#include "editor_app.h"

#include <algorithm>
#include <cmath>
#include <format>

#include <imgui.h>
#include <imgui_internal.h>

#include "sov/core/fs.h"
#include "sov/core/png.h"
#include "sov/games/sandbox.h"

namespace sov::editor_app {

// --- camera ----------------------------------------------------------------------------------

Mat4 EditorCamera::World() const {
  return Mat4::Compose(position, QuatFromEulerDegrees({pitch_deg, yaw_deg, 0}), {1, 1, 1});
}

void EditorCamera::SetFromWorld(const Mat4& world) {
  position = GetTranslation(world);
  const Vec3 fwd = Normalize(TransformDirection(world, {0, 0, -1}));
  pitch_deg = RadToDeg(std::asin(std::clamp(fwd.y, -1.0, 1.0)));
  yaw_deg = RadToDeg(std::atan2(-fwd.x, -fwd.z));
}

void EditorCamera::Fly(const runtime::ActionState& in, double dt) {
  yaw_deg += in.look_yaw_deg;
  pitch_deg = std::clamp(pitch_deg + in.look_pitch_deg, -89.0, 89.0);
  const Quat rot = QuatFromEulerDegrees({pitch_deg, yaw_deg, 0});
  const double speed = speed_mps * (in.sprint ? 3.0 : 1.0);
  const Vec3 move = Rotate(rot, {0, 0, -1}) * in.move_forward + Rotate(rot, {1, 0, 0}) * in.move_right +
                    Vec3{0, 1, 0} * in.move_up;
  position += move * (speed * dt);
}

render::ViewCamera EditorCamera::View() const { return {World(), DegToRad(fov_deg), 0.05, 2000.0}; }

// --- app -------------------------------------------------------------------------------------

EditorApp::EditorApp(platform::Window& window, render::Renderer& renderer, ImGuiLayer& ui,
                     std::unique_ptr<editor::EditorDocument> doc)
    : window_(window), renderer_(renderer), ui_(ui), doc_(std::move(doc)) {
  scene_path_ = doc_->path();
  log_sink_ = AddLogSink([this](const LogRecord& r) {
    log_.push_back({r.level, std::format("{} [{}] {}", r.timestamp.substr(11, 12), r.module, r.message)});
    if (log_.size() > 400) log_.pop_front();
  });
  viewport_.view = renderer_.CreateView(viewport_.desired_w, viewport_.desired_h);
  viewport_.width = viewport_.desired_w;
  viewport_.height = viewport_.desired_h;
  ResetCameraToSceneCamera();
  if (const char* anchors = std::getenv("SOV_UI_ANCHORS"); anchors && *anchors) {
    anchors_.Enable(anchors);
    LogInfo("editor", "UI anchors enabled (test hook): {}", anchors);
  }
}

EditorApp::~EditorApp() {
  doc_->EndPlay();
  renderer_.WaitIdle();
  renderer_.DestroyView(viewport_.view);
  RemoveLogSink(log_sink_);
}

void EditorApp::ResetCameraToSceneCamera() {
  const auto snap = doc_->authoring_world().ExtractRenderSnapshot();
  if (snap.active_camera) camera_.SetFromWorld(snap.active_camera->world);
}

void EditorApp::FocusCameraOnSelection() {
  if (!doc_->selection()) return;
  World& w = doc_->authoring_world();
  const auto m = w.GetWorldMatrix(w.Find(*doc_->selection()));
  if (!m) return;
  const Vec3 target = GetTranslation(*m);
  const Quat rot = QuatFromEulerDegrees({camera_.pitch_deg, camera_.yaw_deg, 0});
  camera_.position = target - Rotate(rot, {0, 0, -1}) * 6.0;
}

void EditorApp::StartPlay() {
  if (doc_->playing()) return;
  if (auto r = doc_->BeginPlay(games::MakeSandboxGame()); !r) LogError("editor", "cannot start play: {}", r.error().Describe());
}

void EditorApp::StopPlay() {
  if (!doc_->playing()) return;
  doc_->EndPlay();
  window_.SetRelativeMouse(false);
  viewport_.flying = false;
}

void EditorApp::Save() {
  if (doc_->playing()) {
    LogWarn("editor", "stop play before saving");
    return;
  }
  if (auto r = doc_->Save(); !r) LogError("editor", "save failed: {}", r.error().Describe());
}

Result<void> EditorApp::Reopen() {
  StopPlay();
  const std::string actor = doc_->actor();
  doc_.reset();  // releases the writer lock and discards unsaved in-memory state
  auto reopened = editor::EditorDocument::Open(scene_path_, actor);
  if (!reopened) return reopened.take_error();
  doc_ = std::move(reopened).value();
  inspector_ = {};
  LogInfo("editor", "reopened {} at revision {}", scene_path_.string(), doc_->revision());
  return Ok();
}

void EditorApp::RequestWindowCapture(std::filesystem::path path) { window_capture_path_ = std::move(path); }
void EditorApp::RequestViewportCapture(std::filesystem::path path) { viewport_capture_path_ = std::move(path); }

std::string EditorApp::status_line() const {
  return std::format("{}  |  revision {}{}  |  {}", doc_->document().name, doc_->revision(),
                     doc_->dirty() ? std::format(" (saved r{}, unsaved changes)", doc_->saved_revision()) : " (saved)",
                     playing() ? "PLAYING" : "editing");
}

void EditorApp::HandleEvent(const SDL_Event& e) {
  ui_.ProcessEvent(e);
  input_.HandleEvent(e, viewport_.flying);
  switch (e.type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED: quit_ = true; break;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: renderer_.NotifyWindowResized(); break;
    case SDL_EVENT_WINDOW_MINIMIZED: renderer_.SetPresentationSuspended(true); break;
    case SDL_EVENT_WINDOW_RESTORED:
    case SDL_EVENT_WINDOW_SHOWN: renderer_.SetPresentationSuspended(false); break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
      if (e.button.button == SDL_BUTTON_RIGHT && viewport_.flying) {
        viewport_.flying = false;
        window_.SetRelativeMouse(false);
      }
      break;
    case SDL_EVENT_KEY_DOWN:
      // Escape releases mouse capture; it never stops play or interrupts a save.
      if (e.key.scancode == SDL_SCANCODE_ESCAPE && viewport_.flying) {
        viewport_.flying = false;
        window_.SetRelativeMouse(false);
      }
      break;
    default: break;
  }
}

void EditorApp::ProcessPicks() {
  if (!viewport_.pick_ticket) return;
  if (auto result = renderer_.TakePick(*viewport_.pick_ticket)) {
    viewport_.pick_ticket.reset();
    viewport_.last_pick = result;
    if (!playing()) doc_->Select(result->entity);
  }
}

void EditorApp::BuildUi() {
  ImGuiViewport* main = ImGui::GetMainViewport();
  const ImGuiID dock = ImGui::DockSpaceOverViewport(0, main, ImGuiDockNodeFlags_PassthruCentralNode);
  if (!layout_built_) {
    layout_built_ = true;
    ImGui::DockBuilderRemoveNode(dock);
    ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock, main->WorkSize);
    ImGuiID center = dock;
    const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.18f, nullptr, &center);
    const ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.27f, nullptr, &center);
    const ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.28f, nullptr, &center);
    ImGui::DockBuilderDockWindow("Hierarchy", left);
    ImGui::DockBuilderDockWindow("Inspector", right);
    ImGui::DockBuilderDockWindow("History", bottom);
    ImGui::DockBuilderDockWindow("Diagnostics", bottom);
    ImGui::DockBuilderDockWindow("Viewport", center);
    ImGui::DockBuilderFinish(dock);
  }
  panels::MainMenu(*this);
  panels::Hierarchy(*this);
  panels::Inspector(*this);
  panels::History(*this);
  panels::Diagnostics(*this);
  panels::Viewport(*this);

  // Shortcuts (ignored while a text field has focus).
  const ImGuiIO& io = ImGui::GetIO();
  if (!io.WantTextInput) {
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) Save();
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z)) doc_->Undo();
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y) || ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z)) {
      doc_->Redo();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && doc_->selection() && !playing()) doc_->Delete(*doc_->selection());
    if (ImGui::IsKeyChordPressed(ImGuiKey_F5)) playing() ? StopPlay() : StartPlay();
    if (ImGui::IsKeyChordPressed(ImGuiKey_F) && !viewport_.flying) FocusCameraOnSelection();
  }
}

void EditorApp::RenderScene() {
  // Snapshot: the authoring world while editing, the play world while playing.
  if (playing()) {
    last_snapshot_ = doc_->play()->world().ExtractRenderSnapshot();
  } else {
    World& w = doc_->authoring_world();
    w.UpdateTransforms();  // includes inspector previews applied to the authoring world
    last_snapshot_ = w.ExtractRenderSnapshot();
  }
  render::ViewCamera cam = camera_.View();
  if (playing()) {
    if (auto play_cam = render::ViewCamera::FromSnapshot(last_snapshot_)) cam = *play_cam;
  }
  render::DrawOptions opt;
  if (!playing() && doc_->selection()) opt.selected_pick_index = last_snapshot_.PickIndexOf(*doc_->selection());
  if (viewport_.pending_click) {
    const auto [x, y] = *viewport_.pending_click;
    viewport_.pick_ticket = renderer_.RequestPick(viewport_.view, x, y);
    viewport_.pending_click.reset();
  }
  renderer_.DrawToView(viewport_.view, last_snapshot_, cam, opt);
}

void EditorApp::ProcessCaptures() {
  if (viewport_capture_path_) {
    auto img = renderer_.CaptureView(viewport_.view);
    if (!img) {
      LogError("editor", "viewport capture failed: {}", img.error().Describe());
    } else if (auto w = WritePng(*viewport_capture_path_, *img,
                                 render::CaptureProvenance(renderer_, "sovereign-editor viewport view readback",
                                                           scene::ProvenanceLabel(doc_->document(), scene_path_)));
               !w) {
      LogError("editor", "cannot write {}: {}", viewport_capture_path_->string(), w.error().Describe());
    } else {
      LogInfo("editor", "viewport capture written: {}", viewport_capture_path_->string());
    }
    viewport_capture_path_.reset();
  }
  if (window_capture_inflight_) {
    window_capture_inflight_ = false;
    auto img = renderer_.TakeWindowCapture();
    if (!img) {
      LogError("editor", "window capture failed: {}", img.error().Describe());
    } else if (auto w = WritePng(*window_capture_path_, *img,
                                 render::CaptureProvenance(renderer_, "sovereign-editor swapchain readback (full editor window)",
                                                           scene::ProvenanceLabel(doc_->document(), scene_path_)));
               !w) {
      LogError("editor", "cannot write {}: {}", window_capture_path_->string(), w.error().Describe());
    } else {
      LogInfo("editor", "window capture written: {}", window_capture_path_->string());
    }
    window_capture_path_.reset();
  }
}

bool EditorApp::Frame(double dt) {
  // Between frames: apply the viewport size requested by last frame's layout.
  if (viewport_.desired_w != viewport_.width || viewport_.desired_h != viewport_.height) {
    if (renderer_.ResizeView(viewport_.view, viewport_.desired_w, viewport_.desired_h)) {
      viewport_.width = viewport_.desired_w;
      viewport_.height = viewport_.desired_h;
    }
  }
  ProcessPicks();

  const runtime::ActionState actions = input_.Consume(viewport_.flying);
  if (playing()) {
    doc_->play()->Advance(dt, viewport_.flying ? actions : runtime::ActionState{});
  } else if (viewport_.flying) {
    camera_.Fly(actions, dt);
  }

  ui_.NewFrame();
  anchors_.BeginFrame();
  BuildUi();
  ui_.Render();
  if (anchors_.enabled()) {
    RecordAnchorState();
    if (frame_ % 3 == 0) anchors_.Flush(frame_);
  }

  last_rendered_ = renderer_.BeginFrame();
  if (last_rendered_) {
    RenderScene();
    if (window_capture_path_ && !window_capture_inflight_) {
      renderer_.RequestWindowCapture();
      window_capture_inflight_ = true;
    }
    renderer_.EndFrame();
  }
  if (last_rendered_ || !window_capture_inflight_) ProcessCaptures();
  ++frame_;
  return !quit_;
}

// --- UI anchors (test hook) -----------------------------------------------------------------

void UiAnchors::BeginFrame() {
  if (enabled()) data_ = Json::object();
}

void UiAnchors::Item(std::string_view group, std::string_view key) {
  if (!enabled()) return;
  const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
  Rect(group, key, a.x, a.y, b.x, b.y);
}

void UiAnchors::Rect(std::string_view group, std::string_view key, float x0, float y0, float x1, float y1) {
  if (!enabled()) return;
  data_[std::string(group)][std::string(key)] = Json::array({x0, y0, x1, y1});
}

void UiAnchors::Point(std::string_view group, std::string_view key, float x, float y) {
  if (!enabled()) return;
  data_[std::string(group)][std::string(key)] = Json::array({x, y});
}

void UiAnchors::State(std::string_view key, Json value) {
  if (!enabled()) return;
  data_["state"][std::string(key)] = std::move(value);
}

void UiAnchors::Flush(std::uint64_t frame) {
  if (!enabled()) return;
  data_["frame"] = frame;
  if (auto r = fs::AtomicWriteFile(path_, CompactJson(data_) + "\n"); !r) {
    LogWarn("editor", "cannot write UI anchors: {}", r.error().Describe());
  }
}

void EditorApp::RecordAnchorState() {
  const auto& d = doc_->document();
  anchors_.State("revision", doc_->revision());
  anchors_.State("dirty", doc_->dirty());
  anchors_.State("playing", playing());
  anchors_.State("entity_count", d.entities.size());
  anchors_.State("history_size", doc_->session().history().size());
  int ww = 0, wh = 0;
  window_.PixelSize(ww, wh);
  anchors_.State("window", Json::array({ww, wh}));
  Json selected = nullptr;
  if (doc_->selection()) {
    if (const scene::EntityData* e = d.Find(*doc_->selection())) {
      selected = Json{{"id", e->id.ToString()}, {"name", e->name}, {"components", scene::ComponentsToJson(*e)}};
    }
  }
  anchors_.State("selected", std::move(selected));
  // Projected centres of rendered entities, in window pixels, so a test can click them.
  if (viewport_.width == 0 || viewport_.height == 0) return;
  const render::ViewCamera cam = camera_.View();
  const Mat4 vp = cam.Projection(double(viewport_.width) / double(viewport_.height)) * cam.ViewMatrix();
  World& w = doc_->authoring_world();
  for (const auto& e : d.entities) {
    if (!e.primitive_mesh) continue;
    const auto wm = w.GetWorldMatrix(w.Find(e.id));
    if (!wm) continue;
    Vec3 ndc;
    if (!ProjectToNdc(vp, GetTranslation(*wm), ndc) || std::abs(ndc.x) > 1 || std::abs(ndc.y) > 1) continue;
    const Vec2 px = NdcToPixel(ndc, viewport_.width, viewport_.height);
    anchors_.Point("viewport_entities", e.name, viewport_.origin_x + static_cast<float>(px.x),
                   viewport_.origin_y + static_cast<float>(px.y));
  }
}

}  // namespace sov::editor_app
