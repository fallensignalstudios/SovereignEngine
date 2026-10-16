#include "sov/rhi/vulkan/device.h"

#include <algorithm>
#include <cstring>
#include <format>

#include "sov/core/log.h"

namespace sov::vk {
namespace {

std::string VersionString(std::uint32_t v) {
  return std::format("{}.{}.{}", VK_API_VERSION_MAJOR(v), VK_API_VERSION_MINOR(v), VK_API_VERSION_PATCH(v));
}

std::string TypeString(VkPhysicalDeviceType t) {
  switch (t) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete-gpu";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated-gpu";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual-gpu";
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return "cpu";
    default: return "other";
  }
}

int TypeScore(VkPhysicalDeviceType t) {
  switch (t) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 4;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 3;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 2;
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return 1;
    default: return 0;
  }
}

bool HasExtension(const std::vector<VkExtensionProperties>& list, const char* name) {
  return std::any_of(list.begin(), list.end(), [&](const auto& e) { return std::strcmp(e.extensionName, name) == 0; });
}

std::vector<VkExtensionProperties> InstanceExtensions(const char* layer) {
  std::uint32_t n = 0;
  vkEnumerateInstanceExtensionProperties(layer, &n, nullptr);
  std::vector<VkExtensionProperties> v(n);
  vkEnumerateInstanceExtensionProperties(layer, &n, v.data());
  return v;
}

}  // namespace

std::string_view ResultString(VkResult r) {
  switch (r) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    default: return "VkResult(other)";
  }
}

Error VkError(VkResult r, std::string_view what) {
  const ErrorCode code = (r == VK_ERROR_DEVICE_LOST) ? ErrorCode::InvalidState : ErrorCode::IoFailure;
  return MakeError(code, std::format("Vulkan: {} failed with {} ({})", what, ResultString(r), static_cast<int>(r)));
}

VKAPI_ATTR VkBool32 VKAPI_CALL Device::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                     VkDebugUtilsMessageTypeFlagsEXT types,
                                                     const VkDebugUtilsMessengerCallbackDataEXT* data, void* user) {
  auto* self = static_cast<Device*>(user);
  const bool is_validation = (types & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) != 0;
  const bool is_error = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0;
  const char* id = data && data->pMessageIdName ? data->pMessageIdName : "-";
  const char* msg = data && data->pMessage ? data->pMessage : "";
  if (is_validation) {
    std::lock_guard lock(self->validation_mutex_);
    if (is_error) {
      ++self->validation_.errors;
    } else {
      ++self->validation_.warnings;
    }
    if (self->validation_.messages.size() < 64) {
      self->validation_.messages.push_back(std::format("[{}] {}: {}", is_error ? "error" : "warning", id, msg));
    }
  }
  // Loader/general messages are logged but not counted as validation findings.
  if (is_error) {
    LogError(is_validation ? "vulkan-validation" : "vulkan", "{}: {}", id, msg);
  } else {
    LogWarn(is_validation ? "vulkan-validation" : "vulkan", "{}: {}", id, msg);
  }
  return VK_FALSE;
}

Result<std::unique_ptr<Device>> Device::Create(const DeviceConfig& config) {
  std::unique_ptr<Device> d(new Device());
  d->windowed_ = static_cast<bool>(config.create_surface);
  if (auto r = d->CreateInstance(config); !r) return r.take_error();
  if (config.create_surface) {
    auto s = config.create_surface(d->instance_);
    if (!s) return s.take_error();
    d->surface_ = *s;
  }
  if (auto r = d->SelectPhysicalDevice(config); !r) return r.take_error();
  if (auto r = d->CreateLogicalDevice(); !r) return r.take_error();
  return d;
}

Result<void> Device::CreateInstance(const DeviceConfig& config) {
  if (config.get_instance_proc_addr) {
    volkInitializeCustom(reinterpret_cast<PFN_vkGetInstanceProcAddr>(config.get_instance_proc_addr));
  } else {
    SOV_VK_TRY(volkInitialize(), "loading the Vulkan loader (libvulkan)");
  }
  const std::uint32_t loader_version = volkGetInstanceVersion();
  info_.instance_version = VersionString(loader_version);
  if (loader_version < VK_API_VERSION_1_3) {
    return MakeError(ErrorCode::UnsupportedComponent,
                     std::format("Vulkan loader reports instance version {}; Vulkan 1.3 is required", info_.instance_version));
  }

  std::vector<const char*> layers;
  std::vector<const char*> extensions = config.instance_extensions;
  const auto available = InstanceExtensions(nullptr);
  bool validation_features = false;
  if (config.validation) {
    std::uint32_t n = 0;
    vkEnumerateInstanceLayerProperties(&n, nullptr);
    std::vector<VkLayerProperties> props(n);
    vkEnumerateInstanceLayerProperties(&n, props.data());
    const bool has_layer = std::any_of(props.begin(), props.end(), [](const auto& p) {
      return std::strcmp(p.layerName, "VK_LAYER_KHRONOS_validation") == 0;
    });
    if (has_layer) {
      layers.push_back("VK_LAYER_KHRONOS_validation");
      validation_.layer_enabled = true;
      const auto layer_exts = InstanceExtensions("VK_LAYER_KHRONOS_validation");
      if (HasExtension(layer_exts, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME)) {
        extensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
        validation_features = true;
      }
    } else {
      LogWarn("vulkan", "validation requested but VK_LAYER_KHRONOS_validation is not installed; continuing WITHOUT validation");
    }
  }
  const bool debug_utils = HasExtension(available, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) || validation_.layer_enabled;
  if (debug_utils) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.pApplicationName = "Sovereign Engine";
  app.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
  app.pEngineName = "Sovereign Engine";
  app.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
  app.apiVersion = VK_API_VERSION_1_3;

  VkDebugUtilsMessengerCreateInfoEXT messenger_info{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
  messenger_info.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  messenger_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  messenger_info.pfnUserCallback = &Device::DebugCallback;
  messenger_info.pUserData = this;

  const VkValidationFeatureEnableEXT enables[] = {VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT};
  VkValidationFeaturesEXT features{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
  features.enabledValidationFeatureCount = 1;
  features.pEnabledValidationFeatures = enables;

  VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  ci.pApplicationInfo = &app;
  ci.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
  ci.ppEnabledLayerNames = layers.data();
  ci.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
  ci.ppEnabledExtensionNames = extensions.data();
  if (debug_utils) {
    ci.pNext = &messenger_info;
    if (validation_features) messenger_info.pNext = &features;
  }
  SOV_VK_TRY(vkCreateInstance(&ci, nullptr, &instance_), "vkCreateInstance");
  volkLoadInstanceOnly(instance_);
  validation_.synchronization_validation = validation_features;
  if (debug_utils) {
    VkDebugUtilsMessengerCreateInfoEXT mi = messenger_info;
    mi.pNext = nullptr;
    SOV_VK_TRY(vkCreateDebugUtilsMessengerEXT(instance_, &mi, nullptr, &messenger_), "vkCreateDebugUtilsMessengerEXT");
  }
  for (auto* l : layers) info_.instance_layers.emplace_back(l);
  for (auto* e : extensions) info_.instance_extensions.emplace_back(e);
  return Ok();
}

Result<void> Device::SelectPhysicalDevice(const DeviceConfig& config) {
  std::uint32_t n = 0;
  SOV_VK_TRY(vkEnumeratePhysicalDevices(instance_, &n, nullptr), "vkEnumeratePhysicalDevices");
  std::vector<VkPhysicalDevice> devices(n);
  SOV_VK_TRY(vkEnumeratePhysicalDevices(instance_, &n, devices.data()), "vkEnumeratePhysicalDevices");
  if (devices.empty()) return MakeError(ErrorCode::UnsupportedComponent, "no Vulkan physical devices are available");

  int best_score = -1;
  for (VkPhysicalDevice pd : devices) {
    VkPhysicalDeviceVulkan12Properties p12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES};
    VkPhysicalDeviceProperties2 p2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    p2.pNext = &p12;
    vkGetPhysicalDeviceProperties2(pd, &p2);
    VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    f12.pNext = &f13;
    VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    f2.pNext = &f12;
    vkGetPhysicalDeviceFeatures2(pd, &f2);

    std::string reject;
    const auto& props = p2.properties;
    if (props.apiVersion < VK_API_VERSION_1_3) reject = "Vulkan 1.3 not supported";
    else if (!f13.dynamicRendering) reject = "dynamicRendering not supported";
    else if (!f13.synchronization2) reject = "synchronization2 not supported";
    else if (!f12.timelineSemaphore) reject = "timelineSemaphore not supported";
    else if (config.require_physical_gpu && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
      reject = "CPU/software implementation refused (physical GPU required)";
    }

    std::uint32_t family = UINT32_MAX;
    if (reject.empty()) {
      std::uint32_t qn = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(pd, &qn, nullptr);
      std::vector<VkQueueFamilyProperties> qf(qn);
      vkGetPhysicalDeviceQueueFamilyProperties(pd, &qn, qf.data());
      for (std::uint32_t i = 0; i < qn && family == UINT32_MAX; ++i) {
        if (!(qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) || !(qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) continue;
        VkBool32 present = VK_TRUE;
        if (surface_) vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, surface_, &present);
        if (present) family = i;
      }
      if (family == UINT32_MAX) reject = "no graphics+compute queue family with present support";
    }
    if (reject.empty() && surface_) {
      std::uint32_t en = 0;
      vkEnumerateDeviceExtensionProperties(pd, nullptr, &en, nullptr);
      std::vector<VkExtensionProperties> exts(en);
      vkEnumerateDeviceExtensionProperties(pd, nullptr, &en, exts.data());
      if (!HasExtension(exts, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) reject = "VK_KHR_swapchain missing";
    }
    const std::string desc = std::format("{} ({}, api {}, driver {} {})", props.deviceName, TypeString(props.deviceType),
                                         VersionString(props.apiVersion), p12.driverName, p12.driverInfo);
    if (!reject.empty()) {
      info_.candidates.push_back(desc + " -> rejected: " + reject);
      continue;
    }
    const int score = TypeScore(props.deviceType);
    info_.candidates.push_back(desc + std::format(" -> eligible (score {})", score));
    if (score > best_score) {
      best_score = score;
      physical_ = pd;
      queue_family_ = family;
      properties_ = props;
      info_.name = props.deviceName;
      info_.device_type = TypeString(props.deviceType);
      info_.is_cpu_implementation = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
      info_.vendor_id = props.vendorID;
      info_.device_id = props.deviceID;
      info_.api_version = VersionString(props.apiVersion);
      info_.driver_name = p12.driverName;
      info_.driver_info = p12.driverInfo;
      info_.conformance_version = std::format("{}.{}.{}.{}", p12.conformanceVersion.major, p12.conformanceVersion.minor,
                                              p12.conformanceVersion.subminor, p12.conformanceVersion.patch);
    }
  }
  for (const auto& c : info_.candidates) LogInfo("vulkan", "physical device: {}", c);
  if (!physical_) {
    std::string all;
    for (const auto& c : info_.candidates) all += "\n  " + c;
    return MakeError(ErrorCode::UnsupportedComponent, "no suitable Vulkan device:" + all);
  }
  return Ok();
}

Result<void> Device::CreateLogicalDevice() {
  const float priority = 1.0f;
  VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  qci.queueFamilyIndex = queue_family_;
  qci.queueCount = 1;
  qci.pQueuePriorities = &priority;

  VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  f13.dynamicRendering = VK_TRUE;
  f13.synchronization2 = VK_TRUE;
  VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  f12.timelineSemaphore = VK_TRUE;
  f12.pNext = &f13;
  VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
  f2.pNext = &f12;
  info_.requested_features = {"apiVersion 1.3", "dynamicRendering", "synchronization2", "timelineSemaphore"};

  std::vector<const char*> exts;
  if (surface_) exts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  VkDeviceCreateInfo ci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  ci.pNext = &f2;
  ci.queueCreateInfoCount = 1;
  ci.pQueueCreateInfos = &qci;
  ci.enabledExtensionCount = static_cast<std::uint32_t>(exts.size());
  ci.ppEnabledExtensionNames = exts.data();
  SOV_VK_TRY(vkCreateDevice(physical_, &ci, nullptr, &device_), "vkCreateDevice");
  volkLoadDevice(device_);
  vkGetDeviceQueue(device_, queue_family_, 0, &queue_);
  for (auto* e : exts) info_.device_extensions.emplace_back(e);

  VmaAllocatorCreateInfo ai{};
  ai.physicalDevice = physical_;
  ai.device = device_;
  ai.instance = instance_;
  ai.vulkanApiVersion = VK_API_VERSION_1_3;
  VmaVulkanFunctions funcs{};
  SOV_VK_TRY(vmaImportVulkanFunctionsFromVolk(&ai, &funcs), "vmaImportVulkanFunctionsFromVolk");
  ai.pVulkanFunctions = &funcs;
  SOV_VK_TRY(vmaCreateAllocator(&ai, &allocator_), "vmaCreateAllocator");

  VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
  type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
  type.initialValue = 0;
  VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  sci.pNext = &type;
  SOV_VK_TRY(vkCreateSemaphore(device_, &sci, nullptr, &timeline_), "creating the submission timeline semaphore");

  VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pci.queueFamilyIndex = queue_family_;
  SOV_VK_TRY(vkCreateCommandPool(device_, &pci, nullptr, &immediate_pool_), "vkCreateCommandPool");

  LogInfo("vulkan", "device: {} [{}] api {} driver {} ({}) conformance {}", info_.name, info_.device_type,
          info_.api_version, info_.driver_name, info_.driver_info, info_.conformance_version);
  LogInfo("vulkan", "requested features: apiVersion 1.3, dynamicRendering, synchronization2, timelineSemaphore; "
          "validation layer {}, synchronization validation {}",
          validation_.layer_enabled ? "ON" : "OFF", validation_.synchronization_validation ? "ON" : "OFF");
  if (info_.is_cpu_implementation) {
    LogWarn("vulkan", "selected device is a CPU (software) Vulkan implementation: results are software-rendered and do "
                      "NOT qualify physical-GPU behaviour or performance");
  }
  return Ok();
}

Device::~Device() {
  if (device_) {
    vkDeviceWaitIdle(device_);
    while (!retired_.empty()) {
      retired_.front().destroy();
      retired_.pop_front();
    }
    if (immediate_pool_) vkDestroyCommandPool(device_, immediate_pool_, nullptr);
    if (timeline_) vkDestroySemaphore(device_, timeline_, nullptr);
    if (allocator_) vmaDestroyAllocator(allocator_);
    vkDestroyDevice(device_, nullptr);
  }
  if (surface_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
  if (messenger_) vkDestroyDebugUtilsMessengerEXT(instance_, messenger_, nullptr);
  if (instance_) vkDestroyInstance(instance_, nullptr);
}

ValidationStats Device::validation() const {
  std::lock_guard lock(validation_mutex_);
  return validation_;
}

Result<std::uint64_t> Device::Submit(VkCommandBuffer cmd, VkSemaphore wait_binary, VkPipelineStageFlags2 wait_stage,
                                     VkSemaphore signal_binary) {
  VkCommandBufferSubmitInfo cbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
  cbi.commandBuffer = cmd;
  VkSemaphoreSubmitInfo wait{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
  wait.semaphore = wait_binary;
  wait.stageMask = wait_stage;
  VkSemaphoreSubmitInfo signals[2] = {{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO}, {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO}};
  const std::uint64_t value = submitted_ + 1;
  signals[0].semaphore = timeline_;
  signals[0].value = value;
  signals[0].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  signals[1].semaphore = signal_binary;
  signals[1].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  VkSubmitInfo2 si{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
  si.waitSemaphoreInfoCount = wait_binary ? 1u : 0u;
  si.pWaitSemaphoreInfos = &wait;
  si.commandBufferInfoCount = 1;
  si.pCommandBufferInfos = &cbi;
  si.signalSemaphoreInfoCount = signal_binary ? 2u : 1u;
  si.pSignalSemaphoreInfos = signals;
  SOV_VK_TRY(vkQueueSubmit2(queue_, 1, &si, VK_NULL_HANDLE), "vkQueueSubmit2");
  submitted_ = value;
  return value;
}

std::uint64_t Device::CompletedValue() const {
  std::uint64_t v = 0;
  vkGetSemaphoreCounterValue(device_, timeline_, &v);
  return v;
}

Result<void> Device::WaitForValue(std::uint64_t value) const {
  if (value == 0) return Ok();
  VkSemaphoreWaitInfo wi{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
  wi.semaphoreCount = 1;
  wi.pSemaphores = &timeline_;
  wi.pValues = &value;
  const VkResult r = vkWaitSemaphores(device_, &wi, 20'000'000'000ULL);  // 20 s: treat as device failure
  if (r != VK_SUCCESS) return VkError(r, std::format("waiting for GPU submission {}", value));
  return Ok();
}

Result<void> Device::ImmediateSubmit(const std::function<void(VkCommandBuffer)>& record) {
  VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  ai.commandPool = immediate_pool_;
  ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  ai.commandBufferCount = 1;
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  SOV_VK_TRY(vkAllocateCommandBuffers(device_, &ai, &cmd), "vkAllocateCommandBuffers");
  VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmd, &bi);
  // Order this one-shot work after everything submitted before it on the queue (memory reuse
  // between uploads/readbacks would otherwise rely only on a host-side completion wait).
  VkMemoryBarrier2 mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
  mb.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  mb.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
  mb.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  mb.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
  VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  dep.memoryBarrierCount = 1;
  dep.pMemoryBarriers = &mb;
  vkCmdPipelineBarrier2(cmd, &dep);
  record(cmd);
  vkEndCommandBuffer(cmd);
  auto value = Submit(cmd, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
  Result<void> waited = value ? WaitForValue(*value) : Result<void>(value.error());
  vkFreeCommandBuffers(device_, immediate_pool_, 1, &cmd);
  return waited;
}

void Device::Retire(std::function<void()> destroy) {
  // +1: the resource may also be referenced by commands recorded but not yet submitted.
  retired_.push_back({submitted_ + 1, std::move(destroy)});
}

void Device::CollectRetired() {
  const std::uint64_t done = CompletedValue();
  while (!retired_.empty() && retired_.front().after_value <= done) {
    retired_.front().destroy();
    retired_.pop_front();
  }
}

void Device::WaitIdle() {
  vkDeviceWaitIdle(device_);
  while (!retired_.empty()) {
    retired_.front().destroy();
    retired_.pop_front();
  }
}

bool Device::SupportsOptimal(VkFormat format, VkFormatFeatureFlags features) const {
  VkFormatProperties p{};
  vkGetPhysicalDeviceFormatProperties(physical_, format, &p);
  return (p.optimalTilingFeatures & features) == features;
}

}  // namespace sov::vk
