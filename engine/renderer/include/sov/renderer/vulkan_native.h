#pragma once

// Backend-specific bridge for the editor's tool-UI overlay (Dear ImGui's Vulkan backend needs raw
// Vulkan objects). Only editor code includes this; the runtime and game modules never do.
#include <functional>

#include "sov/renderer/renderer.h"
#include "sov/rhi/vulkan/vk.h"

namespace sov::render::native {

struct VulkanObjects {
  VkInstance instance = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  std::uint32_t queue_family = 0;
  VkFormat swapchain_format = VK_FORMAT_UNDEFINED;
  std::uint32_t swapchain_min_images = 2;
  std::uint32_t swapchain_images = 2;
};

VulkanObjects Objects(Renderer& renderer);

// Recorded into the window pass after the scene, before present (target is the swapchain image
// in COLOR_ATTACHMENT_OPTIMAL, rendering already begun with dynamic rendering).
void SetOverlayRecorder(Renderer& renderer, std::function<void(VkCommandBuffer)> record);

// The view's tone-mapped output for sampling in the overlay: SHADER_READ_ONLY_OPTIMAL whenever
// the overlay records.
VkImageView ViewOutputImageView(Renderer& renderer, ViewHandle view);

// Defers `destroy` until GPU work submitted so far (and the frame being recorded) has completed.
void RetireAfterGpu(Renderer& renderer, std::function<void()> destroy);

}  // namespace sov::render::native
