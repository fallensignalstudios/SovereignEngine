#pragma once

#include <cstdint>
#include <unordered_map>

#include <SDL3/SDL.h>
#include <imgui.h>

#include "sov/core/result.h"
#include "sov/renderer/renderer.h"

namespace sov::platform {
class Window;
}

namespace sov::editor_app {

// Dear ImGui (docking) on SDL3 + the engine's Vulkan device. Tool UI only.
class ImGuiLayer {
 public:
  Result<void> Init(platform::Window& window, render::Renderer& renderer);
  void Shutdown();
  ~ImGuiLayer() { Shutdown(); }

  void ProcessEvent(const SDL_Event& e);
  void NewFrame();
  // Finalizes the frame's draw data; it is recorded into the window pass at EndFrame.
  void Render();

  // Texture id for a renderer view's output; re-registered when the view's image changes.
  // Replaced descriptor sets are freed only after the GPU has finished with them.
  ImTextureID ViewTexture(render::ViewHandle view);

 private:
  struct Registered {
    std::uint64_t image_generation = 0;
    std::uint64_t descriptor = 0;  // VkDescriptorSet
  };
  render::Renderer* renderer_ = nullptr;
  bool initialized_ = false;
  std::uint32_t min_images_ = 0;
  std::unordered_map<std::uint64_t, Registered> textures_;
};

// Neutral dark graphite with restrained bronze highlights; colors converted for the sRGB swapchain.
void ApplyWorkbenchTheme();

}  // namespace sov::editor_app
