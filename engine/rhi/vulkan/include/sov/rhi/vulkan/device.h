#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "sov/rhi/vulkan/vk.h"

namespace sov::vk {

struct DeviceInfo {
  std::string name;
  std::string device_type;   // "discrete-gpu", "integrated-gpu", "virtual-gpu", "cpu", "other"
  bool is_cpu_implementation = false;
  std::uint32_t vendor_id = 0;
  std::uint32_t device_id = 0;
  std::string api_version;
  std::string driver_name;
  std::string driver_info;
  std::string conformance_version;
  std::string instance_version;
  std::vector<std::string> instance_layers;
  std::vector<std::string> instance_extensions;
  std::vector<std::string> device_extensions;
  std::vector<std::string> requested_features;
  std::vector<std::string> candidates;  // every enumerated physical device and why it was (not) chosen
};

struct ValidationStats {
  bool layer_enabled = false;
  bool synchronization_validation = false;
  std::uint32_t errors = 0;
  std::uint32_t warnings = 0;
  std::vector<std::string> messages;  // first messages, capped
};

struct DeviceConfig {
  bool validation = true;              // VK_LAYER_KHRONOS_validation + synchronization validation if present
  bool require_physical_gpu = false;   // refuse CPU/software implementations
  std::vector<const char*> instance_extensions;  // e.g. surface extensions from the window
  void* get_instance_proc_addr = nullptr;        // optional loader entry (SDL); otherwise volkInitialize()
  std::function<Result<VkSurfaceKHR>(VkInstance)> create_surface;  // optional: windowed device
};

// Instance, physical/logical device, one graphics(+present) queue, VMA, a timeline semaphore for
// submission completion and a retirement queue for resources still referenced by in-flight work.
class Device {
 public:
  static Result<std::unique_ptr<Device>> Create(const DeviceConfig& config);
  ~Device();
  Device(const Device&) = delete;
  Device& operator=(const Device&) = delete;

  VkInstance instance() const { return instance_; }
  VkPhysicalDevice physical() const { return physical_; }
  VkDevice device() const { return device_; }
  VkQueue queue() const { return queue_; }
  std::uint32_t queue_family() const { return queue_family_; }
  VmaAllocator allocator() const { return allocator_; }
  VkSurfaceKHR surface() const { return surface_; }
  const VkPhysicalDeviceProperties& properties() const { return properties_; }
  const DeviceInfo& info() const { return info_; }
  ValidationStats validation() const;

  // Submits one command buffer (synchronization2). Optional binary wait/signal semaphores are for
  // swapchain acquire/present. Always signals the timeline; returns the signalled value.
  Result<std::uint64_t> Submit(VkCommandBuffer cmd, VkSemaphore wait_binary, VkPipelineStageFlags2 wait_stage,
                               VkSemaphore signal_binary);
  std::uint64_t submitted_value() const { return submitted_; }
  std::uint64_t CompletedValue() const;
  Result<void> WaitForValue(std::uint64_t value) const;

  // Records and submits a one-off command buffer, then waits for it (uploads, captures).
  Result<void> ImmediateSubmit(const std::function<void(VkCommandBuffer)>& record);

  // Defers `destroy` until all work submitted so far has completed.
  void Retire(std::function<void()> destroy);
  void CollectRetired();
  std::size_t pending_retirements() const { return retired_.size(); }
  void WaitIdle();

  bool SupportsOptimal(VkFormat format, VkFormatFeatureFlags features) const;

 private:
  Device() = default;
  static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                      VkDebugUtilsMessageTypeFlagsEXT types,
                                                      const VkDebugUtilsMessengerCallbackDataEXT* data, void* user);
  Result<void> CreateInstance(const DeviceConfig& config);
  Result<void> SelectPhysicalDevice(const DeviceConfig& config);
  Result<void> CreateLogicalDevice();

  VkInstance instance_ = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT messenger_ = VK_NULL_HANDLE;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  VkPhysicalDevice physical_ = VK_NULL_HANDLE;
  VkPhysicalDeviceProperties properties_{};
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  std::uint32_t queue_family_ = 0;
  VmaAllocator allocator_ = VK_NULL_HANDLE;
  VkSemaphore timeline_ = VK_NULL_HANDLE;
  std::uint64_t submitted_ = 0;
  VkCommandPool immediate_pool_ = VK_NULL_HANDLE;
  struct Retired {
    std::uint64_t after_value;
    std::function<void()> destroy;
  };
  std::deque<Retired> retired_;
  DeviceInfo info_;
  mutable std::mutex validation_mutex_;
  ValidationStats validation_;
  bool windowed_ = false;
};

}  // namespace sov::vk
