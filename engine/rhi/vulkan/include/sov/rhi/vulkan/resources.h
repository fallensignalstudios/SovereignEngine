#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "sov/rhi/vulkan/device.h"

namespace sov::vk {

struct Buffer {
  VkBuffer buffer = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  VkDeviceSize size = 0;
  void* mapped = nullptr;  // persistently mapped for host-visible buffers
};

// Image with CPU-side tracking of its current layout and last use, so barriers can name the
// correct source scope. All recording happens on one owner thread in submission order.
struct Image {
  VkImage image = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;  // null for swapchain images (not owned)
  VkImageView view = VK_NULL_HANDLE;
  VkFormat format = VK_FORMAT_UNDEFINED;
  VkExtent2D extent{0, 0};
  VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkPipelineStageFlags2 last_stage = VK_PIPELINE_STAGE_2_NONE;
  VkAccessFlags2 last_access = VK_ACCESS_2_NONE;
};

enum class MemoryKind { DeviceLocal, HostUpload, HostReadback };

Result<Buffer> CreateBuffer(Device& d, VkDeviceSize size, VkBufferUsageFlags usage, MemoryKind kind);
void DestroyBuffer(Device& d, Buffer& b);         // immediate (caller guarantees the GPU is done)
void RetireBuffer(Device& d, Buffer& b);          // deferred until in-flight work completes

Result<Image> CreateImage2D(Device& d, VkFormat format, VkExtent2D extent, VkImageUsageFlags usage,
                            VkImageAspectFlags aspect);
void DestroyImage(Device& d, Image& img);
void RetireImage(Device& d, Image& img);
Result<VkImageView> CreateView(Device& d, VkImage image, VkFormat format, VkImageAspectFlags aspect);

// Pipeline barrier from the image's tracked last use to (layout, stage, access).
// Transitioning from UNDEFINED discards contents.
void Transition(VkCommandBuffer cmd, Image& img, VkImageLayout layout, VkPipelineStageFlags2 stage,
                VkAccessFlags2 access, bool discard = false);

// Uploads bytes into a device-local buffer through a staging buffer (blocking).
Result<Buffer> CreateDeviceLocalWithData(Device& d, const void* data, VkDeviceSize size, VkBufferUsageFlags usage);

Result<VkShaderModule> LoadShaderModule(Device& d, const std::filesystem::path& spirv_path);

struct LiveObjectCounts {
  std::int64_t buffers = 0;
  std::int64_t images = 0;
  std::int64_t image_views = 0;
};
LiveObjectCounts LiveObjects();

}  // namespace sov::vk
