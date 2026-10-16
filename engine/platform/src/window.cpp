#include "sov/platform/window.h"

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>

#include <format>

#include "sov/core/log.h"

namespace sov::platform {

Result<std::string> InitializeVideo() {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    return MakeError(ErrorCode::IoFailure, std::format("SDL_Init failed: {}", SDL_GetError()));
  }
  const char* driver = SDL_GetCurrentVideoDriver();
  LogInfo("platform", "SDL {}.{}.{} video driver: {}", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION,
          driver ? driver : "(none)");
  return std::string(driver ? driver : "");
}

void ShutdownVideo() { SDL_Quit(); }

Result<std::unique_ptr<Window>> Window::Create(const WindowDesc& desc) {
  SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
  if (desc.resizable) flags |= SDL_WINDOW_RESIZABLE;
  SDL_Window* w = SDL_CreateWindow(desc.title.c_str(), desc.width, desc.height, flags);
  if (!w) return MakeError(ErrorCode::IoFailure, std::format("SDL_CreateWindow failed: {}", SDL_GetError()));
  std::unique_ptr<Window> window(new Window());
  window->window_ = w;
  const char* driver = SDL_GetCurrentVideoDriver();
  window->video_driver_ = driver ? driver : "";
  return window;
}

Window::~Window() {
  if (window_) SDL_DestroyWindow(window_);
}

void Window::PixelSize(int& w, int& h) const {
  if (!SDL_GetWindowSizeInPixels(window_, &w, &h)) w = h = 0;
}

bool Window::minimized() const { return (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) != 0; }
void Window::SetSize(int w, int h) { SDL_SetWindowSize(window_, w, h); }
void Window::Minimize() { SDL_MinimizeWindow(window_); }
void Window::Restore() { SDL_RestoreWindow(window_); }
void Window::SetTitle(const std::string& title) { SDL_SetWindowTitle(window_, title.c_str()); }
void Window::SetRelativeMouse(bool enabled) { SDL_SetWindowRelativeMouseMode(window_, enabled); }

std::vector<const char*> Window::VulkanInstanceExtensions() const {
  Uint32 count = 0;
  const char* const* names = SDL_Vulkan_GetInstanceExtensions(&count);
  return names ? std::vector<const char*>(names, names + count) : std::vector<const char*>{};
}

void* Window::VulkanGetInstanceProcAddr() const { return reinterpret_cast<void*>(SDL_Vulkan_GetVkGetInstanceProcAddr()); }

Result<std::uint64_t> Window::CreateVulkanSurface(void* instance) const {
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  if (!SDL_Vulkan_CreateSurface(window_, static_cast<VkInstance>(instance), nullptr, &surface)) {
    return MakeError(ErrorCode::IoFailure, std::format("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError()));
  }
  return reinterpret_cast<std::uint64_t>(surface);
}

void ActionMapper::HandleEvent(const SDL_Event& e, bool captured) {
  if (e.type == SDL_EVENT_MOUSE_MOTION && captured) {
    mouse_dx_ += e.motion.xrel;
    mouse_dy_ += e.motion.yrel;
  }
}

runtime::ActionState ActionMapper::Consume(bool keyboard_enabled) {
  runtime::ActionState s;
  if (keyboard_enabled) {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    const auto axis = [&](SDL_Scancode pos, SDL_Scancode neg) { return (keys[pos] ? 1.0 : 0.0) - (keys[neg] ? 1.0 : 0.0); };
    s.move_forward = axis(SDL_SCANCODE_W, SDL_SCANCODE_S);
    s.move_right = axis(SDL_SCANCODE_D, SDL_SCANCODE_A);
    s.move_up = axis(SDL_SCANCODE_E, SDL_SCANCODE_Q);
    s.sprint = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
  }
  s.look_yaw_deg = -mouse_dx_ * sensitivity_;
  s.look_pitch_deg = -mouse_dy_ * sensitivity_;
  mouse_dx_ = mouse_dy_ = 0;
  return s;
}

}  // namespace sov::platform
