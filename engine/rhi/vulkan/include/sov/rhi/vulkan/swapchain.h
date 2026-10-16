#pragma once

#include <memory>
#include <vector>

#include "sov/rhi/vulkan/resources.h"

namespace sov::vk {

// Swapchain for the device's surface. Frame-context indexing (the renderer's frame ring) is
// independent of the acquired image index; present semaphores are per swapchain image.
class Swapchain {
 public:
  static Result<std::unique_ptr<Swapchain>> Create(Device& d, std::uint32_t width, std::uint32_t height, bool vsync);
  ~Swapchain();

  // Waits for the device to go idle, then rebuilds images and semaphores for the new extent.
  Result<void> Recreate(std::uint32_t width, std::uint32_t height);

  enum class Status { Ok, Suboptimal, OutOfDate, Error };
  Status Acquire(VkSemaphore signal, std::uint32_t& image_index);
  Status Present(std::uint32_t image_index);

  VkFormat format() const { return format_; }
  VkExtent2D extent() const { return extent_; }
  std::uint32_t image_count() const { return static_cast<std::uint32_t>(images_.size()); }
  std::uint32_t min_image_count() const { return min_image_count_; }
  Image& image(std::uint32_t i) { return images_[i]; }
  VkSemaphore render_finished(std::uint32_t i) const { return render_finished_[i]; }
  bool supports_transfer_src() const { return transfer_src_; }
  VkPresentModeKHR present_mode() const { return present_mode_; }
  std::uint64_t generation() const { return generation_; }

 private:
  explicit Swapchain(Device& d) : device_(d) {}
  Result<void> Build(std::uint32_t width, std::uint32_t height);
  void DestroyImageResources();

  Device& device_;
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  VkFormat format_ = VK_FORMAT_UNDEFINED;
  VkColorSpaceKHR color_space_ = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  VkExtent2D extent_{0, 0};
  VkPresentModeKHR present_mode_ = VK_PRESENT_MODE_FIFO_KHR;
  std::uint32_t min_image_count_ = 2;
  bool vsync_ = true;
  bool transfer_src_ = false;
  std::vector<Image> images_;
  std::vector<VkSemaphore> render_finished_;
  std::uint64_t generation_ = 0;
};

}  // namespace sov::vk
