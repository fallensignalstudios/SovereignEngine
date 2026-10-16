#include "sov/rhi/vulkan/swapchain.h"

#include <algorithm>
#include <format>

#include "sov/core/log.h"

namespace sov::vk {

Result<std::unique_ptr<Swapchain>> Swapchain::Create(Device& d, std::uint32_t width, std::uint32_t height, bool vsync) {
  std::unique_ptr<Swapchain> s(new Swapchain(d));
  s->vsync_ = vsync;
  if (auto r = s->Build(width, height); !r) return r.take_error();
  return s;
}

Swapchain::~Swapchain() {
  device_.WaitIdle();
  DestroyImageResources();
  if (swapchain_) vkDestroySwapchainKHR(device_.device(), swapchain_, nullptr);
}

void Swapchain::DestroyImageResources() {
  for (auto& img : images_) {
    if (img.view) vkDestroyImageView(device_.device(), img.view, nullptr);
  }
  images_.clear();
  for (auto s : render_finished_) vkDestroySemaphore(device_.device(), s, nullptr);
  render_finished_.clear();
}

Result<void> Swapchain::Build(std::uint32_t width, std::uint32_t height) {
  VkPhysicalDevice pd = device_.physical();
  VkSurfaceKHR surface = device_.surface();
  VkSurfaceCapabilitiesKHR caps{};
  SOV_VK_TRY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surface, &caps), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

  std::uint32_t n = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &n, nullptr);
  std::vector<VkSurfaceFormatKHR> formats(n);
  vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &n, formats.data());
  // One output encoding step: an sRGB swapchain format encodes linear shader output in hardware.
  VkSurfaceFormatKHR chosen{VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
  for (VkFormat want : {VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB}) {
    for (const auto& f : formats) {
      if (f.format == want && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) chosen = f;
    }
    if (chosen.format != VK_FORMAT_UNDEFINED) break;
  }
  if (chosen.format == VK_FORMAT_UNDEFINED) {
    return MakeError(ErrorCode::UnsupportedComponent, "surface offers no B8G8R8A8_SRGB/R8G8B8A8_SRGB format");
  }

  vkGetPhysicalDeviceSurfacePresentModesKHR(pd, surface, &n, nullptr);
  std::vector<VkPresentModeKHR> modes(n);
  vkGetPhysicalDeviceSurfacePresentModesKHR(pd, surface, &n, modes.data());
  VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;  // always supported
  if (!vsync_) {
    for (VkPresentModeKHR m : {VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR}) {
      if (std::find(modes.begin(), modes.end(), m) != modes.end()) {
        mode = m;
        break;
      }
    }
  }

  VkExtent2D extent = caps.currentExtent;
  if (extent.width == UINT32_MAX) {
    extent.width = std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent.height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
  }
  if (extent.width == 0 || extent.height == 0) {
    return MakeError(ErrorCode::InvalidState, "surface extent is zero (window minimized)");
  }
  std::uint32_t count = std::max(caps.minImageCount + 1, 3u);
  if (caps.maxImageCount > 0) count = std::min(count, caps.maxImageCount);

  transfer_src_ = (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
  VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  if (transfer_src_) usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

  VkSwapchainKHR old = swapchain_;
  VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  ci.surface = surface;
  ci.minImageCount = count;
  ci.imageFormat = chosen.format;
  ci.imageColorSpace = chosen.colorSpace;
  ci.imageExtent = extent;
  ci.imageArrayLayers = 1;
  ci.imageUsage = usage;
  ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ci.preTransform = caps.currentTransform;
  ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  ci.presentMode = mode;
  ci.clipped = VK_TRUE;
  ci.oldSwapchain = old;
  SOV_VK_TRY(vkCreateSwapchainKHR(device_.device(), &ci, nullptr, &swapchain_), "vkCreateSwapchainKHR");
  if (old) vkDestroySwapchainKHR(device_.device(), old, nullptr);

  std::uint32_t image_n = 0;
  vkGetSwapchainImagesKHR(device_.device(), swapchain_, &image_n, nullptr);
  std::vector<VkImage> raw(image_n);
  vkGetSwapchainImagesKHR(device_.device(), swapchain_, &image_n, raw.data());
  for (VkImage vi : raw) {
    Image img;
    img.image = vi;
    img.format = chosen.format;
    img.extent = extent;
    img.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vci.image = vi;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = chosen.format;
    vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    SOV_VK_TRY(vkCreateImageView(device_.device(), &vci, nullptr, &img.view), "swapchain image view");
    images_.push_back(img);
    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore s = VK_NULL_HANDLE;
    SOV_VK_TRY(vkCreateSemaphore(device_.device(), &sci, nullptr, &s), "present semaphore");
    render_finished_.push_back(s);
  }
  format_ = chosen.format;
  color_space_ = chosen.colorSpace;
  extent_ = extent;
  present_mode_ = mode;
  min_image_count_ = count;
  ++generation_;
  LogInfo("vulkan", "swapchain {}x{} format {} images {} present mode {} transfer_src {}", extent.width, extent.height,
          static_cast<int>(format_), image_n, static_cast<int>(mode), transfer_src_);
  return Ok();
}

Result<void> Swapchain::Recreate(std::uint32_t width, std::uint32_t height) {
  device_.WaitIdle();
  DestroyImageResources();
  return Build(width, height);
}

Swapchain::Status Swapchain::Acquire(VkSemaphore signal, std::uint32_t& index) {
  const VkResult r = vkAcquireNextImageKHR(device_.device(), swapchain_, 5'000'000'000ULL, signal, VK_NULL_HANDLE, &index);
  if (r == VK_SUCCESS) return Status::Ok;
  if (r == VK_SUBOPTIMAL_KHR) return Status::Suboptimal;
  if (r == VK_ERROR_OUT_OF_DATE_KHR) return Status::OutOfDate;
  LogError("vulkan", "vkAcquireNextImageKHR: {}", ResultString(r));
  return Status::Error;
}

Swapchain::Status Swapchain::Present(std::uint32_t index) {
  VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  pi.waitSemaphoreCount = 1;
  pi.pWaitSemaphores = &render_finished_[index];
  pi.swapchainCount = 1;
  pi.pSwapchains = &swapchain_;
  pi.pImageIndices = &index;
  const VkResult r = vkQueuePresentKHR(device_.queue(), &pi);
  if (r == VK_SUCCESS) return Status::Ok;
  if (r == VK_SUBOPTIMAL_KHR) return Status::Suboptimal;
  if (r == VK_ERROR_OUT_OF_DATE_KHR) return Status::OutOfDate;
  LogError("vulkan", "vkQueuePresentKHR: {}", ResultString(r));
  return Status::Error;
}

}  // namespace sov::vk
