#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "sov/core/result.h"
#include "sov/runtime/simulation.h"

// Platform layer over SDL3: window, event pump, and the input action map. SDL's renderer and
// SDL_GPU are not used (and are disabled at build time); Vulkan surfaces are created from here.
namespace sov::platform {

struct WindowDesc {
  std::string title = "Sovereign Engine";
  int width = 1600;
  int height = 900;
  bool resizable = true;
};

class Window {
 public:
  static Result<std::unique_ptr<Window>> Create(const WindowDesc& desc);
  ~Window();
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  SDL_Window* sdl() const { return window_; }
  void PixelSize(int& w, int& h) const;
  bool minimized() const;
  SDL_WindowID id() const { return SDL_GetWindowID(window_); }

  void SetSize(int w, int h);
  void Minimize();
  void Restore();
  void SetTitle(const std::string& title);
  void SetRelativeMouse(bool enabled);

  // Vulkan integration (types erased so this header needs no Vulkan include).
  std::vector<const char*> VulkanInstanceExtensions() const;
  void* VulkanGetInstanceProcAddr() const;
  // Returns VkSurfaceKHR (as uint64) for `instance` (VkInstance as void*).
  Result<std::uint64_t> CreateVulkanSurface(void* instance) const;

  const std::string& video_driver() const { return video_driver_; }

 private:
  Window() = default;
  SDL_Window* window_ = nullptr;
  std::string video_driver_;
};

// Maps SDL keyboard/mouse state to named actions. Gameplay sees only ActionState.
class ActionMapper {
 public:
  void HandleEvent(const SDL_Event& e, bool captured);
  // Builds the action state for this frame and resets per-frame deltas.
  runtime::ActionState Consume(bool keyboard_enabled);
  void SetLookSensitivity(double deg_per_pixel) { sensitivity_ = deg_per_pixel; }

 private:
  double mouse_dx_ = 0, mouse_dy_ = 0;
  double sensitivity_ = 0.15;
};

// Initializes SDL video/events once per process; returns the video driver name.
Result<std::string> InitializeVideo();
void ShutdownVideo();

}  // namespace sov::platform
