#pragma once

// Vulkan backend private header. Only engine/rhi/vulkan, engine/renderer and the editor's
// overlay bridge include this; game modules and the world never see Vulkan types.
#include <volk.h>

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include <vk_mem_alloc.h>

#include <string>
#include <string_view>

#include "sov/core/result.h"

namespace sov::vk {

std::string_view ResultString(VkResult r);
Error VkError(VkResult r, std::string_view what);

}  // namespace sov::vk

#define SOV_VK_TRY(expr, what)                                       \
  do {                                                               \
    const VkResult sov_vk_result_ = (expr);                          \
    if (sov_vk_result_ != VK_SUCCESS) return ::sov::vk::VkError(sov_vk_result_, what); \
  } while (0)
