#include "sov/rhi/vulkan/resources.h"

#include <atomic>
#include <cstring>
#include <format>

#include "sov/core/fs.h"

namespace sov::vk {
namespace {

std::atomic<std::int64_t> g_buffers{0}, g_images{0}, g_views{0};

}  // namespace

LiveObjectCounts LiveObjects() { return {g_buffers.load(), g_images.load(), g_views.load()}; }

Result<Buffer> CreateBuffer(Device& d, VkDeviceSize size, VkBufferUsageFlags usage, MemoryKind kind) {
  VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bi.size = size;
  bi.usage = usage;
  bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  VmaAllocationCreateInfo ai{};
  ai.usage = VMA_MEMORY_USAGE_AUTO;
  if (kind == MemoryKind::HostUpload) {
    ai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
  } else if (kind == MemoryKind::HostReadback) {
    ai.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
  }
  Buffer b;
  b.size = size;
  VmaAllocationInfo info{};
  SOV_VK_TRY(vmaCreateBuffer(d.allocator(), &bi, &ai, &b.buffer, &b.allocation, &info), "vmaCreateBuffer");
  b.mapped = info.pMappedData;
  ++g_buffers;
  return b;
}

void DestroyBuffer(Device& d, Buffer& b) {
  if (b.buffer) {
    vmaDestroyBuffer(d.allocator(), b.buffer, b.allocation);
    --g_buffers;
  }
  b = {};
}

void RetireBuffer(Device& d, Buffer& b) {
  if (!b.buffer) return;
  Buffer copy = b;
  d.Retire([&d, copy]() mutable { DestroyBuffer(d, copy); });
  b = {};
}

Result<VkImageView> CreateView(Device& d, VkImage image, VkFormat format, VkImageAspectFlags aspect) {
  VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  vi.image = image;
  vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vi.format = format;
  vi.subresourceRange = {aspect, 0, 1, 0, 1};
  VkImageView view = VK_NULL_HANDLE;
  SOV_VK_TRY(vkCreateImageView(d.device(), &vi, nullptr, &view), "vkCreateImageView");
  ++g_views;
  return view;
}

Result<Image> CreateImage2D(Device& d, VkFormat format, VkExtent2D extent, VkImageUsageFlags usage,
                            VkImageAspectFlags aspect) {
  VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  ii.imageType = VK_IMAGE_TYPE_2D;
  ii.format = format;
  ii.extent = {extent.width, extent.height, 1};
  ii.mipLevels = 1;
  ii.arrayLayers = 1;
  ii.samples = VK_SAMPLE_COUNT_1_BIT;
  ii.tiling = VK_IMAGE_TILING_OPTIMAL;
  ii.usage = usage;
  ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  VmaAllocationCreateInfo ai{};
  ai.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  Image img;
  img.format = format;
  img.extent = extent;
  img.aspect = aspect;
  SOV_VK_TRY(vmaCreateImage(d.allocator(), &ii, &ai, &img.image, &img.allocation, nullptr), "vmaCreateImage");
  ++g_images;
  auto view = CreateView(d, img.image, format, aspect);
  if (!view) {
    DestroyImage(d, img);
    return view.take_error();
  }
  img.view = *view;
  return img;
}

void DestroyImage(Device& d, Image& img) {
  if (img.view) {
    vkDestroyImageView(d.device(), img.view, nullptr);
    --g_views;
  }
  if (img.image && img.allocation) {
    vmaDestroyImage(d.allocator(), img.image, img.allocation);
    --g_images;
  }
  img = {};
}

void RetireImage(Device& d, Image& img) {
  if (!img.image && !img.view) return;
  Image copy = img;
  d.Retire([&d, copy]() mutable { DestroyImage(d, copy); });
  img = {};
}

void Transition(VkCommandBuffer cmd, Image& img, VkImageLayout layout, VkPipelineStageFlags2 stage,
                VkAccessFlags2 access, bool discard) {
  VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
  b.srcStageMask = img.last_stage == VK_PIPELINE_STAGE_2_NONE ? VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT : img.last_stage;
  b.srcAccessMask = img.last_access;
  b.dstStageMask = stage;
  b.dstAccessMask = access;
  b.oldLayout = discard ? VK_IMAGE_LAYOUT_UNDEFINED : img.layout;
  b.newLayout = layout;
  b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.image = img.image;
  b.subresourceRange = {img.aspect, 0, 1, 0, 1};
  VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  dep.imageMemoryBarrierCount = 1;
  dep.pImageMemoryBarriers = &b;
  vkCmdPipelineBarrier2(cmd, &dep);
  img.layout = layout;
  img.last_stage = stage;
  img.last_access = access;
}

Result<Buffer> CreateDeviceLocalWithData(Device& d, const void* data, VkDeviceSize size, VkBufferUsageFlags usage) {
  auto staging = CreateBuffer(d, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryKind::HostUpload);
  if (!staging) return staging.take_error();
  std::memcpy(staging->mapped, data, static_cast<std::size_t>(size));
  vmaFlushAllocation(d.allocator(), staging->allocation, 0, VK_WHOLE_SIZE);
  auto dst = CreateBuffer(d, size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, MemoryKind::DeviceLocal);
  if (!dst) {
    DestroyBuffer(d, *staging);
    return dst.take_error();
  }
  const VkBuffer src_buf = staging->buffer, dst_buf = dst->buffer;
  auto r = d.ImmediateSubmit([&](VkCommandBuffer cmd) {
    const VkBufferCopy region{0, 0, size};
    vkCmdCopyBuffer(cmd, src_buf, dst_buf, 1, &region);
    VkMemoryBarrier2 mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    mb.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
    mb.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    mb.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
    mb.dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_2_INDEX_READ_BIT;
    VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.memoryBarrierCount = 1;
    dep.pMemoryBarriers = &mb;
    vkCmdPipelineBarrier2(cmd, &dep);
  });
  DestroyBuffer(d, *staging);
  if (!r) {
    DestroyBuffer(d, *dst);
    return r.take_error();
  }
  return dst;
}

Result<VkShaderModule> LoadShaderModule(Device& d, const std::filesystem::path& path) {
  auto bytes = fs::ReadFile(path, 8u * 1024u * 1024u);
  if (!bytes) {
    return MakeError(ErrorCode::IoFailure, std::format("cannot read shader {}: {}", path.string(), bytes.error().message));
  }
  if (bytes->size() % 4 != 0 || bytes->size() < 20) {
    return MakeError(ErrorCode::SchemaError, std::format("{} is not a SPIR-V binary", path.string()));
  }
  std::vector<std::uint32_t> words(bytes->size() / 4);
  std::memcpy(words.data(), bytes->data(), bytes->size());
  if (words[0] != 0x07230203u) return MakeError(ErrorCode::SchemaError, std::format("{}: bad SPIR-V magic", path.string()));
  VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  ci.codeSize = bytes->size();
  ci.pCode = words.data();
  VkShaderModule module = VK_NULL_HANDLE;
  SOV_VK_TRY(vkCreateShaderModule(d.device(), &ci, nullptr, &module), "vkCreateShaderModule " + path.filename().string());
  return module;
}

}  // namespace sov::vk
