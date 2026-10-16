// sovereign-editor: native authoring editor (Dear ImGui tool UI on the engine's Vulkan renderer).
//
//   sovereign-editor --scene PATH [--width W --height H] [--automation SCRIPT.json] [--no-validation]
//
// The scene's writer lock is held while the editor runs, so offline CLI writes are refused.
// Exit codes: 0 ok, 1 cannot open scene, 2 usage, 4 platform/graphics failure,
//             5 automation step failed, 6 Vulkan validation errors were reported.

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

#include "automation.h"
#include "editor_app.h"
#include "imgui_layer.h"
#include "sov/core/build_info.h"
#include "sov/core/log.h"

int main(int argc, char** argv) {
  using namespace sov;
  std::string scene_path, automation_path;
  int width = 1600, height = 900;
  bool validation = true;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--version") {
      std::fputs(DescribeBuild("sovereign-editor").c_str(), stdout);
      return 0;
    } else if (a == "--scene" && i + 1 < argc) {
      scene_path = argv[++i];
    } else if (a == "--automation" && i + 1 < argc) {
      automation_path = argv[++i];
    } else if (a == "--width" && i + 1 < argc) {
      width = std::atoi(argv[++i]);
    } else if (a == "--height" && i + 1 < argc) {
      height = std::atoi(argv[++i]);
    } else if (a == "--no-validation") {
      validation = false;
    } else {
      std::fputs("usage: sovereign-editor --scene PATH [--width W --height H] [--automation SCRIPT.json] [--no-validation]\n", stderr);
      return 2;
    }
  }
  if (scene_path.empty()) {
    std::fputs("sovereign-editor: --scene is required\n", stderr);
    return 2;
  }

  auto doc = editor::EditorDocument::Open(scene_path, "editor:local");
  if (!doc) {
    LogError("editor", "cannot open {}: {}", scene_path, doc.error().Describe());
    return 1;
  }
  std::optional<editor_app::AutomationRunner> automation;
  if (!automation_path.empty()) {
    auto loaded = editor_app::AutomationRunner::Load(automation_path);
    if (!loaded) {
      LogError("editor", "cannot load automation {}: {}", automation_path, loaded.error().Describe());
      return 2;
    }
    automation = std::move(loaded).value();
  }

  auto driver = platform::InitializeVideo();
  if (!driver) {
    LogError("editor", "{}", driver.error().Describe());
    return 4;
  }
  int exit_code = 0;
  {
    auto window = platform::Window::Create({"Sovereign Editor - " + (*doc)->document().name, width, height, true});
    if (!window) {
      LogError("editor", "{}", window.error().Describe());
      platform::ShutdownVideo();
      return 4;
    }
    render::RendererConfig rc;
    rc.validation = validation;
    auto renderer = render::Renderer::CreateForWindow(rc, **window);
    if (!renderer) {
      LogError("editor", "{}", renderer.error().Describe());
      platform::ShutdownVideo();
      return 4;
    }
    LogInfo("editor", "evidence class: {}", (*renderer)->device().evidence_class);
    {
      editor_app::ImGuiLayer ui;
      if (auto r = ui.Init(**window, **renderer); !r) {
        LogError("editor", "{}", r.error().Describe());
        platform::ShutdownVideo();
        return 4;
      }
      {
        editor_app::EditorApp app(**window, **renderer, ui, std::move(doc).value());
        auto last = std::chrono::steady_clock::now();
        bool running = true;
        while (running) {
          SDL_Event e;
          while (SDL_PollEvent(&e)) app.HandleEvent(e);
          const auto now = std::chrono::steady_clock::now();
          const double dt = std::min(0.25, std::chrono::duration<double>(now - last).count());
          last = now;
          running = app.Frame(dt);
          if (automation && !automation->Tick(app)) {
            if (automation->failures() > 0) exit_code = 5;
            running = false;
          }
          if (!app.last_frame_rendered()) SDL_Delay(5);
        }
        if (app.doc().dirty()) {
          LogWarn("editor", "closing with unsaved changes at revision {} (saved revision {}); they are discarded",
                  app.doc().revision(), app.doc().saved_revision());
        }
      }
      ui.Shutdown();
    }
    const auto v = (*renderer)->validation();
    LogInfo("editor", "validation: layer {}, synchronization {}, {} error(s), {} warning(s)", v.layer_enabled ? "on" : "off",
            v.synchronization_validation ? "on" : "off", v.errors, v.warnings);
    if (v.errors > 0 && exit_code == 0) exit_code = 6;
  }
  platform::ShutdownVideo();
  return exit_code;
}
