// sovereign-runtime: standalone runtime. Opens a validated scene document (G1 reads authoring JSON;
// cooked packages are a G2 requirement), runs the sandbox game module at a fixed 60 Hz tick and
// renders through the native Vulkan backend. Contains no editor, tool-UI or authoring-command code.
//
//   sovereign-runtime --scene PATH [--width W --height H] [--frames N] [--screenshot OUT.png]
//                     [--scripted-input] [--stress-window] [--no-validation] [--no-vsync]
//
// Exit codes: 0 ok, 1 scene invalid, 2 usage, 4 platform/graphics failure, 6 validation errors seen,
//             7 resource growth detected by --stress-window.

#include <chrono>
#include <cstdio>
#include <cstring>
#include <format>
#include <string>

#include "sov/core/build_info.h"
#include "sov/core/log.h"
#include "sov/core/png.h"
#include "sov/games/sandbox.h"
#include "sov/platform/window.h"
#include "sov/renderer/renderer.h"
#include "sov/runtime/simulation.h"
#include "sov/world/scene_document.h"

namespace {

using namespace sov;

struct Options {
  std::string scene;
  int width = 1600, height = 900;
  long frames = -1;  // -1 = until the window closes
  std::string screenshot;
  bool scripted_input = false;
  bool stress_window = false;
  bool validation = true;
  bool vsync = true;
};

bool Parse(int argc, char** argv, Options& o) {
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    const auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : nullptr; };
    if (a == "--scene") {
      if (const char* v = next()) o.scene = v; else return false;
    } else if (a == "--width") {
      if (const char* v = next()) o.width = std::atoi(v); else return false;
    } else if (a == "--height") {
      if (const char* v = next()) o.height = std::atoi(v); else return false;
    } else if (a == "--frames") {
      if (const char* v = next()) o.frames = std::atol(v); else return false;
    } else if (a == "--screenshot") {
      if (const char* v = next()) o.screenshot = v; else return false;
    } else if (a == "--scripted-input") {
      o.scripted_input = true;
    } else if (a == "--stress-window") {
      o.stress_window = true;
    } else if (a == "--no-validation") {
      o.validation = false;
    } else if (a == "--no-vsync") {
      o.vsync = false;
    } else {
      return false;
    }
  }
  return !o.scene.empty() && o.width > 0 && o.height > 0;
}

void PrintSummary(render::Renderer& r, const runtime::Simulation& sim, std::uint64_t frames, std::uint64_t skipped) {
  const auto v = r.validation();
  const auto res = r.resources();
  std::fprintf(stdout,
               "runtime summary: frames=%llu skipped=%llu ticks=%llu dropped_sim_seconds=%.3f\n"
               "device: %s [%s] driver %s\n"
               "evidence class: %s\n"
               "validation: layer %s, sync %s, errors %u, warnings %u\n"
               "resources: buffers %lld images %lld views %lld vma allocations %llu\n",
               static_cast<unsigned long long>(frames), static_cast<unsigned long long>(skipped),
               static_cast<unsigned long long>(sim.tick_count()), sim.clock().dropped_seconds(), r.device().name.c_str(),
               r.device().device_type.c_str(), r.device().driver.c_str(), r.device().evidence_class.c_str(),
               v.layer_enabled ? "on" : "off", v.synchronization_validation ? "on" : "off", v.errors, v.warnings,
               static_cast<long long>(res.buffers), static_cast<long long>(res.images),
               static_cast<long long>(res.image_views), static_cast<unsigned long long>(res.vma_allocations));
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 2 && std::strcmp(argv[1], "--version") == 0) {
    std::fputs(DescribeBuild("sovereign-runtime").c_str(), stdout);
    return 0;
  }
  Options opt;
  if (!Parse(argc, argv, opt)) {
    std::fputs("usage: sovereign-runtime --scene PATH [--width W --height H] [--frames N] [--screenshot OUT.png]\n"
               "                         [--scripted-input] [--stress-window] [--no-validation] [--no-vsync]\n",
               stderr);
    return 2;
  }

  auto loaded = scene::LoadSceneFile(opt.scene);
  if (!loaded) {
    LogError("runtime", "cannot open scene {}: {}", opt.scene, loaded.error().Describe());
    return 1;
  }
  for (const auto& w : loaded->warnings) LogWarn("runtime", "{}", w);
  LogInfo("runtime", "scene \"{}\" ({}), revision {}, {} entities, file sha256 {}", loaded->document.name,
          loaded->document.scene_id.ToString(), loaded->document.revision, loaded->document.entities.size(),
          loaded->file_sha256);

  auto sim = runtime::Simulation::Create(loaded->document, WorldKind::Runtime, games::MakeSandboxGame());
  if (!sim) {
    LogError("runtime", "cannot build runtime world: {}", sim.error().Describe());
    return 1;
  }

  auto driver = platform::InitializeVideo();
  if (!driver) {
    LogError("runtime", "{}", driver.error().Describe());
    return 4;
  }
  int exit_code = 0;
  {
    auto window = platform::Window::Create({"Sovereign Runtime - " + loaded->document.name, opt.width, opt.height, true});
    if (!window) {
      LogError("runtime", "{}", window.error().Describe());
      platform::ShutdownVideo();
      return 4;
    }
    render::RendererConfig rc;
    rc.validation = opt.validation;
    rc.vsync = opt.vsync;
    auto renderer = render::Renderer::CreateForWindow(rc, **window);
    if (!renderer) {
      LogError("runtime", "{}", renderer.error().Describe());
      platform::ShutdownVideo();
      return 4;
    }
    render::Renderer& r = **renderer;
    LogInfo("runtime", "evidence class: {}", r.device().evidence_class);

    platform::ActionMapper input;
    bool captured = false;
    bool running = true;
    std::uint64_t frame = 0, skipped = 0;
    auto last = std::chrono::steady_clock::now();
    render::ResourceSummary baseline{};
    bool baseline_taken = false;
    int stress_step = 0;
    const int stress_sizes[][2] = {{1280, 720}, {640, 360}, {1920, 1080}, {800, 600}, {1600, 900}};

    while (running) {
      SDL_Event e;
      while (SDL_PollEvent(&e)) {
        input.HandleEvent(e, captured);
        switch (e.type) {
          case SDL_EVENT_QUIT:
          case SDL_EVENT_WINDOW_CLOSE_REQUESTED: running = false; break;
          case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: r.NotifyWindowResized(); break;
          case SDL_EVENT_WINDOW_MINIMIZED: r.SetPresentationSuspended(true); break;
          case SDL_EVENT_WINDOW_RESTORED:
          case SDL_EVENT_WINDOW_SHOWN: r.SetPresentationSuspended(false); break;
          case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (e.button.button == SDL_BUTTON_RIGHT && !captured) {
              captured = true;
              (*window)->SetRelativeMouse(true);
            }
            break;
          case SDL_EVENT_KEY_DOWN:
            if (e.key.scancode == SDL_SCANCODE_ESCAPE) {
              if (captured) {  // Escape releases capture first; a second press quits
                captured = false;
                (*window)->SetRelativeMouse(false);
              } else {
                running = false;
              }
            }
            break;
          default: break;
        }
      }
      const auto now = std::chrono::steady_clock::now();
      double dt = std::chrono::duration<double>(now - last).count();
      last = now;
      runtime::ActionState actions = input.Consume(true);
      if (opt.scripted_input) {
        // Deterministic flight for unattended evidence runs: forward along the walkway, slow turn.
        actions = {};
        actions.move_forward = 1.0;
        actions.look_yaw_deg = frame % 240 < 120 ? 0.05 : -0.05;
        dt = 1.0 / 60.0;
      }
      (*sim)->Advance(dt, actions);

      if (opt.stress_window && frame % 20 == 10) {
        // Window lifecycle stress: resize through several sizes, then minimize/restore, repeatedly.
        const int step = stress_step++;
        if (step % 6 == 5) {
          (*window)->Minimize();
        } else if (step % 6 == 0 && step > 0) {
          (*window)->Restore();
        } else {
          const auto& s = stress_sizes[step % 5];
          (*window)->SetSize(s[0], s[1]);
        }
      }

      if (r.BeginFrame()) {
        const RenderSnapshot snap = (*sim)->world().ExtractRenderSnapshot();
        if (auto cam = render::ViewCamera::FromSnapshot(snap)) {
          r.DrawToWindow(snap, *cam);
        } else {
          LogWarn("runtime", "scene has no active camera; nothing drawn");
        }
        const bool want_shot = !opt.screenshot.empty() && opt.frames > 0 && static_cast<long>(frame) == opt.frames - 1;
        if (want_shot) r.RequestWindowCapture();
        r.EndFrame();
        if (want_shot) {
          auto shot = r.TakeWindowCapture();
          if (!shot) {
            LogError("runtime", "screenshot failed: {}", shot.error().Describe());
            exit_code = 4;
          } else if (auto w = WritePng(opt.screenshot, *shot,
                                       render::CaptureProvenance(r, "sovereign-runtime swapchain readback (presented frame)",
                                                                 scene::ProvenanceLabel(loaded->document, opt.scene)));
                     !w) {
            LogError("runtime", "cannot write screenshot: {}", w.error().Describe());
            exit_code = 4;
          } else {
            LogInfo("runtime", "screenshot written: {} ({}x{})", opt.screenshot, shot->width, shot->height);
          }
        }
      } else {
        ++skipped;
        SDL_Delay(5);
      }
      if (opt.stress_window && frame == 5) {
        r.WaitIdle();
        baseline = r.resources();
        baseline_taken = true;
      }
      ++frame;
      if (opt.frames > 0 && static_cast<long>(frame) >= opt.frames) running = false;
    }

    r.WaitIdle();
    PrintSummary(r, **sim, frame, skipped);
    if (opt.stress_window && baseline_taken) {
      // Restore the original size so per-target allocations are comparable with the baseline.
      (*window)->Restore();
      (*window)->SetSize(opt.width, opt.height);
      for (int i = 0; i < 30; ++i) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
          if (e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) r.NotifyWindowResized();
          if (e.type == SDL_EVENT_WINDOW_RESTORED) r.SetPresentationSuspended(false);
        }
        if (r.BeginFrame()) {
          const RenderSnapshot snap = (*sim)->world().ExtractRenderSnapshot();
          if (auto cam = render::ViewCamera::FromSnapshot(snap)) r.DrawToWindow(snap, *cam);
          r.EndFrame();
        }
        SDL_Delay(5);
      }
      r.WaitIdle();
      const auto after = r.resources();
      std::fprintf(stdout, "stress-window: %d lifecycle steps; resources baseline %lld/%lld/%llu -> after %lld/%lld/%llu (buffers/images/allocations)\n",
                   stress_step, static_cast<long long>(baseline.buffers), static_cast<long long>(baseline.images),
                   static_cast<unsigned long long>(baseline.vma_allocations), static_cast<long long>(after.buffers),
                   static_cast<long long>(after.images), static_cast<unsigned long long>(after.vma_allocations));
      if (after.buffers > baseline.buffers || after.images > baseline.images ||
          after.vma_allocations > baseline.vma_allocations) {
        LogError("runtime", "resource growth detected after window stress");
        exit_code = 7;
      }
    }
    if (r.validation().errors > 0 && exit_code == 0) exit_code = 6;
  }
  platform::ShutdownVideo();
  return exit_code;
}
