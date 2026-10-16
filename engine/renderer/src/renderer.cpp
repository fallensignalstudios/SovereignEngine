#include "sov/renderer/renderer.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <format>
#include <map>

#include "gpu_types.h"
#include "sov/core/build_info.h"
#include "sov/core/fs.h"
#include "sov/core/log.h"
#include "sov/platform/window.h"
#include "sov/renderer/vulkan_native.h"
#include "sov/rhi/vulkan/device.h"
#include "sov/rhi/vulkan/resources.h"
#include "sov/rhi/vulkan/swapchain.h"
#include "sov/world/primitive_geometry.h"

namespace sov::render {
namespace {

constexpr std::uint32_t kFramesInFlight = 2;
constexpr std::uint32_t kMaxDrawsPerFrame = 4;
constexpr std::uint32_t kMaxInstancesPerDraw = 10240;  // >= SceneLimits::kMaxEntities
constexpr std::uint32_t kMaxPicksPerFrame = 16;
constexpr VkFormat kHdrFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr VkFormat kPickFormat = VK_FORMAT_R32_UINT;
constexpr VkFormat kDepthFormat = VK_FORMAT_D32_SFLOAT;
constexpr VkFormat kViewOutputFormat = VK_FORMAT_R8G8B8A8_SRGB;

VkDeviceSize AlignUp(VkDeviceSize v, VkDeviceSize a) { return a ? (v + a - 1) / a * a : v; }

void HostReadBarrier(VkCommandBuffer cmd) {
  VkMemoryBarrier2 mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
  mb.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  mb.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
  mb.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
  mb.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
  VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  dep.memoryBarrierCount = 1;
  dep.pMemoryBarriers = &mb;
  vkCmdPipelineBarrier2(cmd, &dep);
}

// Readback buffers are reused across frames: order a new copy into them after earlier copies on
// the queue (the earlier copies' barriers only targeted host reads).
void ReadbackWriteAfterWriteBarrier(VkCommandBuffer cmd) {
  VkMemoryBarrier2 mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
  mb.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
  mb.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
  mb.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
  mb.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
  VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  dep.memoryBarrierCount = 1;
  dep.pMemoryBarriers = &mb;
  vkCmdPipelineBarrier2(cmd, &dep);
}

struct Targets {
  vk::Image hdr, pick, depth;
  VkDescriptorSet tonemap_set = VK_NULL_HANDLE;
  VkExtent2D extent{0, 0};
};

struct View {
  std::uint32_t generation = 0;  // 0 = free slot
  bool alive = false;
  bool drawn = false;
  Targets targets;
  vk::Image output;
  std::uint64_t image_generation = 0;
};

struct PendingPick {
  std::uint64_t ticket = 0;
  ViewHandle view;
  std::uint32_t x = 0, y = 0;
  std::vector<Uuid> table;
  bool recorded = false;
  std::uint32_t frame = 0;
  std::uint32_t slot = 0;
  std::uint64_t submit_value = 0;
};

struct FrameContext {
  VkCommandPool pool = VK_NULL_HANDLE;
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  VkSemaphore image_available = VK_NULL_HANDLE;
  std::uint64_t submit_value = 0;
  vk::Buffer ubo_arena;
  vk::Buffer instance_arena;
  vk::Buffer pick_readback;
  vk::Buffer window_capture;
  VkDescriptorSet scene_set = VK_NULL_HANDLE;
  std::uint32_t draws_used = 0;
  std::uint32_t picks_used = 0;
};

struct ShapeRange {
  std::uint32_t first_index = 0;
  std::uint32_t index_count = 0;
  std::int32_t vertex_offset = 0;
  std::uint32_t triangles = 0;
};

bool EnvFlag(const char* name) {
  const char* v = std::getenv(name);
  return v && std::strcmp(v, "0") != 0 && v[0] != '\0';
}

}  // namespace

struct Renderer::Impl {
  RendererConfig config;
  std::unique_ptr<vk::Device> device;
  std::unique_ptr<vk::Swapchain> swapchain;
  platform::Window* window = nullptr;
  DeviceDescription description;

  VkDescriptorPool pool = VK_NULL_HANDLE;
  VkDescriptorSetLayout scene_set_layout = VK_NULL_HANDLE, tonemap_set_layout = VK_NULL_HANDLE,
                        probe_set_layout = VK_NULL_HANDLE;
  VkPipelineLayout forward_layout = VK_NULL_HANDLE, tonemap_layout = VK_NULL_HANDLE, probe_layout = VK_NULL_HANDLE;
  VkPipeline forward_pipeline = VK_NULL_HANDLE, probe_pipeline = VK_NULL_HANDLE;
  std::map<VkFormat, VkPipeline> tonemap_pipelines;
  VkShaderModule forward_vs = VK_NULL_HANDLE, forward_ps = VK_NULL_HANDLE, tonemap_vs = VK_NULL_HANDLE,
                 tonemap_ps = VK_NULL_HANDLE, probe_cs = VK_NULL_HANDLE;

  vk::Buffer vertices, indices;
  ShapeRange shapes[3];

  FrameContext frames[kFramesInFlight];
  std::uint32_t frame_index = 0;
  std::uint64_t frame_number = 0;
  bool frame_open = false;
  bool window_acquired = false;
  bool window_drawn = false;
  std::uint32_t image_index = 0;
  bool suspended = false;
  bool needs_recreate = false;
  Targets window_targets;

  VkDeviceSize ubo_stride = 0;
  VkDeviceSize instance_stride = 0;

  std::vector<View> views;
  std::vector<PendingPick> picks;
  std::map<std::uint64_t, PickResult> completed_picks;
  std::uint64_t next_ticket = 1;

  std::function<void(VkCommandBuffer)> overlay;
  bool capture_requested = false;
  bool capture_pending = false;
  std::uint64_t capture_value = 0;
  std::uint32_t capture_frame = 0;
  VkExtent2D capture_extent{};
  VkFormat capture_format = VK_FORMAT_UNDEFINED;

  FrameStats stats;
  std::chrono::steady_clock::time_point frame_start;

  // --- setup ---------------------------------------------------------------------------------
  Result<void> Init();
  Result<VkPipeline> TonemapPipeline(VkFormat format);
  Result<void> CreateTargets(Targets& t, VkExtent2D extent);
  void RetireTargets(Targets& t);
  void DestroyTargetsNow(Targets& t);
  Result<void> CreateViewOutput(View& v, VkExtent2D extent);
  Result<void> RecreateSwapchain();
  void ResolvePicks();
  View* Find(ViewHandle h);
  void DrawScene(Targets& t, vk::Image& output, VkFormat output_format, bool view_output, ViewHandle view,
                 const RenderSnapshot& snap, const ViewCamera& cam, const DrawOptions& opt);
  Result<vk::Buffer> ReadbackImage(vk::Image& img, VkImageAspectFlags aspect, VkDeviceSize bytes, VkImageLayout restore,
                                   VkPipelineStageFlags2 restore_stage, VkAccessFlags2 restore_access);
  ~Impl();
};

// ---------------------------------------------------------------------------------------------

Result<std::unique_ptr<Renderer>> Renderer::CreateForWindow(const RendererConfig& config, platform::Window& window) {
  std::unique_ptr<Renderer> r(new Renderer());
  r->impl_ = std::make_unique<Impl>();
  Impl& im = *r->impl_;
  im.config = config;
  im.window = &window;
  vk::DeviceConfig dc;
  dc.validation = config.validation;
  dc.require_physical_gpu = config.require_physical_gpu || EnvFlag("SOV_REQUIRE_PHYSICAL_GPU");
  dc.instance_extensions = window.VulkanInstanceExtensions();
  dc.get_instance_proc_addr = window.VulkanGetInstanceProcAddr();
  dc.create_surface = [&window](VkInstance instance) -> Result<VkSurfaceKHR> {
    auto s = window.CreateVulkanSurface(instance);
    if (!s) return s.take_error();
    return reinterpret_cast<VkSurfaceKHR>(*s);
  };
  auto device = vk::Device::Create(dc);
  if (!device) return device.take_error();
  im.device = std::move(device).value();
  if (auto i = im.Init(); !i) return i.take_error();
  int w = 0, h = 0;
  window.PixelSize(w, h);
  auto sc = vk::Swapchain::Create(*im.device, static_cast<std::uint32_t>(std::max(w, 1)),
                                  static_cast<std::uint32_t>(std::max(h, 1)), config.vsync);
  if (!sc) return sc.take_error();
  im.swapchain = std::move(sc).value();
  if (auto t = im.CreateTargets(im.window_targets, im.swapchain->extent()); !t) return t.take_error();
  im.description.windowed = true;
  im.description.swapchain = std::format("{}x{} format {} images {} present mode {}", im.swapchain->extent().width,
                                         im.swapchain->extent().height, static_cast<int>(im.swapchain->format()),
                                         im.swapchain->image_count(), static_cast<int>(im.swapchain->present_mode()));
  return r;
}

Result<std::unique_ptr<Renderer>> Renderer::CreateOffscreen(const RendererConfig& config) {
  std::unique_ptr<Renderer> r(new Renderer());
  r->impl_ = std::make_unique<Impl>();
  Impl& im = *r->impl_;
  im.config = config;
  vk::DeviceConfig dc;
  dc.validation = config.validation;
  dc.require_physical_gpu = config.require_physical_gpu || EnvFlag("SOV_REQUIRE_PHYSICAL_GPU");
  auto device = vk::Device::Create(dc);
  if (!device) return device.take_error();
  im.device = std::move(device).value();
  if (auto i = im.Init(); !i) return i.take_error();
  return r;
}

Renderer::~Renderer() = default;

Result<void> Renderer::Impl::Init() {
  vk::Device& d = *device;
  const auto& info = d.info();
  description.name = info.name;
  description.device_type = info.device_type;
  description.is_cpu_implementation = info.is_cpu_implementation;
  description.api_version = info.api_version;
  description.driver = info.driver_name + " " + info.driver_info;
  description.conformance_version = info.conformance_version;
  description.instance_version = info.instance_version;
  description.layers = info.instance_layers;
  description.requested_features = info.requested_features;
  description.candidates = info.candidates;
  description.evidence_class = info.is_cpu_implementation ? "software-rendered (CPU Vulkan implementation)"
                                                          : "gpu-rendered (physical GPU)";

  const struct {
    VkFormat format;
    VkFormatFeatureFlags features;
    const char* use;
  } required[] = {
      {kHdrFormat, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT, "HDR scene color"},
      {kPickFormat, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT, "pick IDs"},
      {kDepthFormat, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, "reversed-Z depth"},
      {kViewOutputFormat, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT, "view output"},
  };
  for (const auto& r : required) {
    if (!d.SupportsOptimal(r.format, r.features)) {
      return MakeError(ErrorCode::UnsupportedComponent,
                       std::format("device lacks required format {} for {}", static_cast<int>(r.format), r.use));
    }
  }

  const VkDescriptorPoolSize sizes[] = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, kFramesInFlight},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, kFramesInFlight},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 256},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8},
  };
  VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  pci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pci.maxSets = 160;
  pci.poolSizeCount = static_cast<std::uint32_t>(std::size(sizes));
  pci.pPoolSizes = sizes;
  SOV_VK_TRY(vkCreateDescriptorPool(d.device(), &pci, nullptr, &pool), "vkCreateDescriptorPool");

  const auto make_set_layout = [&](std::initializer_list<VkDescriptorSetLayoutBinding> bindings,
                                   VkDescriptorSetLayout& out) -> Result<void> {
    std::vector<VkDescriptorSetLayoutBinding> b(bindings);
    VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    ci.bindingCount = static_cast<std::uint32_t>(b.size());
    ci.pBindings = b.data();
    SOV_VK_TRY(vkCreateDescriptorSetLayout(d.device(), &ci, nullptr, &out), "vkCreateDescriptorSetLayout");
    return Ok();
  };
  constexpr VkShaderStageFlags kVsFs = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  if (auto r = make_set_layout({{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, kVsFs, nullptr},
                                {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, kVsFs, nullptr}},
                               scene_set_layout);
      !r)
    return r;
  if (auto r = make_set_layout({{0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
                                {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}},
                               tonemap_set_layout);
      !r)
    return r;
  if (auto r = make_set_layout({{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
                                {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
                                {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}},
                               probe_set_layout);
      !r)
    return r;

  const auto make_layout = [&](VkDescriptorSetLayout set, VkShaderStageFlags push_stages, std::uint32_t push_size,
                               VkPipelineLayout& out) -> Result<void> {
    VkPushConstantRange range{push_stages, 0, push_size};
    VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    ci.setLayoutCount = 1;
    ci.pSetLayouts = &set;
    ci.pushConstantRangeCount = push_size ? 1u : 0u;
    ci.pPushConstantRanges = &range;
    SOV_VK_TRY(vkCreatePipelineLayout(d.device(), &ci, nullptr, &out), "vkCreatePipelineLayout");
    return Ok();
  };
  if (auto r = make_layout(scene_set_layout, kVsFs, 4, forward_layout); !r) return r;
  if (auto r = make_layout(tonemap_set_layout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(gpu::TonemapPush), tonemap_layout); !r)
    return r;
  if (auto r = make_layout(probe_set_layout, 0, 0, probe_layout); !r) return r;

  const std::filesystem::path dir = config.shader_dir.empty() ? fs::ExecutableDirectory() / "shaders" : config.shader_dir;
  const auto load = [&](const char* name, VkShaderModule& out) -> Result<void> {
    auto m = vk::LoadShaderModule(d, dir / name);
    if (!m) return m.take_error();
    out = *m;
    return Ok();
  };
  for (auto [name, mod] : {std::pair{"forward.VSMain.spv", &forward_vs}, std::pair{"forward.PSMain.spv", &forward_ps},
                           std::pair{"tonemap.VSMain.spv", &tonemap_vs}, std::pair{"tonemap.PSMain.spv", &tonemap_ps},
                           std::pair{"matrix_probe.CSMain.spv", &probe_cs}}) {
    if (auto r = load(name, *mod); !r) return r;
  }

  // Forward pipeline: reversed-Z (clear 0, GREATER_OR_EQUAL), back-face culling, CCW front faces.
  {
    const VkPipelineShaderStageCreateInfo stages[] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, forward_vs, "VSMain", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, forward_ps, "PSMain", nullptr}};
    const VkVertexInputBindingDescription binding{0, sizeof(PrimitiveVertex), VK_VERTEX_INPUT_RATE_VERTEX};
    // UVs (offset 24) stay in the vertex format for later textured materials; R0 shaders do not read them.
    const VkVertexInputAttributeDescription attrs[] = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
                                                       {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12}};
    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &binding;
    vi.vertexAttributeDescriptionCount = 2;
    vi.pVertexAttributeDescriptions = attrs;
    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
    // Identical states for both attachments: the independentBlend feature is not required.
    VkPipelineColorBlendAttachmentState blend[2]{};
    blend[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend[1] = blend[0];
    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 2;
    cb.pAttachments = blend;
    const VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dy{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dy.dynamicStateCount = 2;
    dy.pDynamicStates = dyn;
    const VkFormat colors[] = {kHdrFormat, kPickFormat};
    VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rendering.colorAttachmentCount = 2;
    rendering.pColorAttachmentFormats = colors;
    rendering.depthAttachmentFormat = kDepthFormat;
    VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    gp.pNext = &rendering;
    gp.stageCount = 2;
    gp.pStages = stages;
    gp.pVertexInputState = &vi;
    gp.pInputAssemblyState = &ia;
    gp.pViewportState = &vp;
    gp.pRasterizationState = &rs;
    gp.pMultisampleState = &ms;
    gp.pDepthStencilState = &ds;
    gp.pColorBlendState = &cb;
    gp.pDynamicState = &dy;
    gp.layout = forward_layout;
    SOV_VK_TRY(vkCreateGraphicsPipelines(d.device(), VK_NULL_HANDLE, 1, &gp, nullptr, &forward_pipeline),
               "creating the forward pipeline");
  }
  {
    VkComputePipelineCreateInfo cp{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cp.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, probe_cs,
                "CSMain", nullptr};
    cp.layout = probe_layout;
    SOV_VK_TRY(vkCreateComputePipelines(d.device(), VK_NULL_HANDLE, 1, &cp, nullptr, &probe_pipeline),
               "creating the matrix probe pipeline");
  }

  // Built-in primitive geometry in one vertex/index buffer pair.
  std::vector<PrimitiveVertex> all_vertices;
  std::vector<std::uint32_t> all_indices;
  for (auto shape : {scene::PrimitiveShape::Box, scene::PrimitiveShape::Plane, scene::PrimitiveShape::Cylinder}) {
    const PrimitiveGeometry g = BuildPrimitive(shape);
    ShapeRange& r = shapes[static_cast<int>(shape)];
    r.first_index = static_cast<std::uint32_t>(all_indices.size());
    r.index_count = static_cast<std::uint32_t>(g.indices.size());
    r.vertex_offset = static_cast<std::int32_t>(all_vertices.size());
    r.triangles = r.index_count / 3;
    all_vertices.insert(all_vertices.end(), g.vertices.begin(), g.vertices.end());
    all_indices.insert(all_indices.end(), g.indices.begin(), g.indices.end());
  }
  auto vb = vk::CreateDeviceLocalWithData(d, all_vertices.data(), all_vertices.size() * sizeof(PrimitiveVertex),
                                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
  if (!vb) return vb.take_error();
  vertices = *vb;
  auto ib = vk::CreateDeviceLocalWithData(d, all_indices.data(), all_indices.size() * sizeof(std::uint32_t),
                                          VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
  if (!ib) return ib.take_error();
  indices = *ib;

  ubo_stride = AlignUp(sizeof(gpu::FrameConstants), d.properties().limits.minUniformBufferOffsetAlignment);
  instance_stride = AlignUp(VkDeviceSize{kMaxInstancesPerDraw} * sizeof(gpu::InstanceData),
                            d.properties().limits.minStorageBufferOffsetAlignment);
  for (auto& f : frames) {
    VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpi.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    cpi.queueFamilyIndex = d.queue_family();
    SOV_VK_TRY(vkCreateCommandPool(d.device(), &cpi, nullptr, &f.pool), "frame command pool");
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = f.pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    SOV_VK_TRY(vkAllocateCommandBuffers(d.device(), &ai, &f.cmd), "frame command buffer");
    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    SOV_VK_TRY(vkCreateSemaphore(d.device(), &sci, nullptr, &f.image_available), "acquire semaphore");
    auto ubo = vk::CreateBuffer(d, ubo_stride * kMaxDrawsPerFrame, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, vk::MemoryKind::HostUpload);
    if (!ubo) return ubo.take_error();
    f.ubo_arena = *ubo;
    auto inst = vk::CreateBuffer(d, instance_stride * kMaxDrawsPerFrame, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                 vk::MemoryKind::HostUpload);
    if (!inst) return inst.take_error();
    f.instance_arena = *inst;
    auto pick = vk::CreateBuffer(d, kMaxPicksPerFrame * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, vk::MemoryKind::HostReadback);
    if (!pick) return pick.take_error();
    f.pick_readback = *pick;
    VkDescriptorSetAllocateInfo dai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dai.descriptorPool = pool;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &scene_set_layout;
    SOV_VK_TRY(vkAllocateDescriptorSets(d.device(), &dai, &f.scene_set), "scene descriptor set");
    const VkDescriptorBufferInfo ubo_info{f.ubo_arena.buffer, 0, sizeof(gpu::FrameConstants)};
    const VkDescriptorBufferInfo inst_info{f.instance_arena.buffer, 0, VkDeviceSize{kMaxInstancesPerDraw} * sizeof(gpu::InstanceData)};
    VkWriteDescriptorSet writes[2] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
    writes[0].dstSet = f.scene_set;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    writes[0].pBufferInfo = &ubo_info;
    writes[1] = writes[0];
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    writes[1].pBufferInfo = &inst_info;
    vkUpdateDescriptorSets(d.device(), 2, writes, 0, nullptr);
  }
  LogInfo("renderer", "R0 forward renderer ready on {} ({}); shaders from {}", description.name,
          description.evidence_class, dir.string());
  return Ok();
}

Result<VkPipeline> Renderer::Impl::TonemapPipeline(VkFormat format) {
  if (auto it = tonemap_pipelines.find(format); it != tonemap_pipelines.end()) return it->second;
  const VkPipelineShaderStageCreateInfo stages[] = {
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, tonemap_vs, "VSMain", nullptr},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, tonemap_ps, "PSMain", nullptr}};
  VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  vp.viewportCount = 1;
  vp.scissorCount = 1;
  VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.cullMode = VK_CULL_MODE_NONE;
  rs.lineWidth = 1.0f;
  VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  VkPipelineColorBlendAttachmentState blend{};
  blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  cb.attachmentCount = 1;
  cb.pAttachments = &blend;
  const VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dy{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dy.dynamicStateCount = 2;
  dy.pDynamicStates = dyn;
  VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  rendering.colorAttachmentCount = 1;
  rendering.pColorAttachmentFormats = &format;
  VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  gp.pNext = &rendering;
  gp.stageCount = 2;
  gp.pStages = stages;
  gp.pVertexInputState = &vi;
  gp.pInputAssemblyState = &ia;
  gp.pViewportState = &vp;
  gp.pRasterizationState = &rs;
  gp.pMultisampleState = &ms;
  gp.pDepthStencilState = &ds;
  gp.pColorBlendState = &cb;
  gp.pDynamicState = &dy;
  gp.layout = tonemap_layout;
  VkPipeline p = VK_NULL_HANDLE;
  SOV_VK_TRY(vkCreateGraphicsPipelines(device->device(), VK_NULL_HANDLE, 1, &gp, nullptr, &p), "creating a tonemap pipeline");
  tonemap_pipelines[format] = p;
  return p;
}

Result<void> Renderer::Impl::CreateTargets(Targets& t, VkExtent2D extent) {
  vk::Device& d = *device;
  auto hdr = vk::CreateImage2D(d, kHdrFormat, extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                               VK_IMAGE_ASPECT_COLOR_BIT);
  if (!hdr) return hdr.take_error();
  auto pick = vk::CreateImage2D(d, kPickFormat, extent,
                                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                VK_IMAGE_ASPECT_COLOR_BIT);
  if (!pick) return pick.take_error();
  auto depth = vk::CreateImage2D(d, kDepthFormat, extent,
                                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                 VK_IMAGE_ASPECT_DEPTH_BIT);
  if (!depth) return depth.take_error();
  t.hdr = *hdr;
  t.pick = *pick;
  t.depth = *depth;
  t.extent = extent;
  VkDescriptorSetAllocateInfo dai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  dai.descriptorPool = pool;
  dai.descriptorSetCount = 1;
  dai.pSetLayouts = &tonemap_set_layout;
  SOV_VK_TRY(vkAllocateDescriptorSets(d.device(), &dai, &t.tonemap_set), "tonemap descriptor set");
  const VkDescriptorImageInfo images[2] = {{VK_NULL_HANDLE, t.hdr.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                                           {VK_NULL_HANDLE, t.pick.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
  VkWriteDescriptorSet w[2] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
  for (std::uint32_t i = 0; i < 2; ++i) {
    w[i].dstSet = t.tonemap_set;
    w[i].dstBinding = i;
    w[i].descriptorCount = 1;
    w[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    w[i].pImageInfo = &images[i];
  }
  vkUpdateDescriptorSets(d.device(), 2, w, 0, nullptr);
  return Ok();
}

void Renderer::Impl::RetireTargets(Targets& t) {
  vk::RetireImage(*device, t.hdr);
  vk::RetireImage(*device, t.pick);
  vk::RetireImage(*device, t.depth);
  if (t.tonemap_set) {
    VkDescriptorSet set = t.tonemap_set;
    VkDevice dev = device->device();
    VkDescriptorPool p = pool;
    device->Retire([dev, p, set] { vkFreeDescriptorSets(dev, p, 1, &set); });
  }
  t = {};
}

void Renderer::Impl::DestroyTargetsNow(Targets& t) {
  vk::DestroyImage(*device, t.hdr);
  vk::DestroyImage(*device, t.pick);
  vk::DestroyImage(*device, t.depth);
  if (t.tonemap_set) vkFreeDescriptorSets(device->device(), pool, 1, &t.tonemap_set);
  t = {};
}

Result<void> Renderer::Impl::CreateViewOutput(View& v, VkExtent2D extent) {
  auto out = vk::CreateImage2D(*device, kViewOutputFormat, extent,
                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                               VK_IMAGE_ASPECT_COLOR_BIT);
  if (!out) return out.take_error();
  v.output = *out;
  // Start in the sampled layout (cleared) so an overlay may display a view before its first draw.
  vk::Image& img = v.output;
  return device->ImmediateSubmit([&](VkCommandBuffer cmd) {
    vk::Transition(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_CLEAR_BIT,
                   VK_ACCESS_2_TRANSFER_WRITE_BIT, true);
    const VkClearColorValue black{{0, 0, 0, 1}};
    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(cmd, img.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &range);
    vk::Transition(cmd, img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                   VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
  });
}

Renderer::Impl::~Impl() {
  if (!device) return;
  device->WaitIdle();
  for (auto& v : views) {
    if (!v.alive) continue;
    DestroyTargetsNow(v.targets);
    vk::DestroyImage(*device, v.output);
  }
  DestroyTargetsNow(window_targets);
  VkDevice dev = device->device();
  for (auto& f : frames) {
    vk::DestroyBuffer(*device, f.ubo_arena);
    vk::DestroyBuffer(*device, f.instance_arena);
    vk::DestroyBuffer(*device, f.pick_readback);
    vk::DestroyBuffer(*device, f.window_capture);
    if (f.image_available) vkDestroySemaphore(dev, f.image_available, nullptr);
    if (f.pool) vkDestroyCommandPool(dev, f.pool, nullptr);
  }
  vk::DestroyBuffer(*device, vertices);
  vk::DestroyBuffer(*device, indices);
  for (auto& [fmt, p] : tonemap_pipelines) vkDestroyPipeline(dev, p, nullptr);
  if (forward_pipeline) vkDestroyPipeline(dev, forward_pipeline, nullptr);
  if (probe_pipeline) vkDestroyPipeline(dev, probe_pipeline, nullptr);
  for (VkShaderModule m : {forward_vs, forward_ps, tonemap_vs, tonemap_ps, probe_cs}) {
    if (m) vkDestroyShaderModule(dev, m, nullptr);
  }
  for (VkPipelineLayout l : {forward_layout, tonemap_layout, probe_layout}) {
    if (l) vkDestroyPipelineLayout(dev, l, nullptr);
  }
  for (VkDescriptorSetLayout l : {scene_set_layout, tonemap_set_layout, probe_set_layout}) {
    if (l) vkDestroyDescriptorSetLayout(dev, l, nullptr);
  }
  if (pool) vkDestroyDescriptorPool(dev, pool, nullptr);
  swapchain.reset();
  device->WaitIdle();
  const auto live = vk::LiveObjects();
  if (live.buffers || live.images || live.image_views) {
    LogWarn("renderer", "resources still alive at shutdown: {} buffers, {} images, {} views", live.buffers, live.images,
            live.image_views);
  }
  device.reset();
}

// --- info ------------------------------------------------------------------------------------

const DeviceDescription& Renderer::device() const { return impl_->description; }

ValidationSummary Renderer::validation() const {
  const auto v = impl_->device->validation();
  return {v.layer_enabled, v.synchronization_validation, v.errors, v.warnings, v.messages};
}

ResourceSummary Renderer::resources() const {
  ResourceSummary s;
  const auto live = vk::LiveObjects();
  s.buffers = live.buffers;
  s.images = live.images;
  s.image_views = live.image_views;
  for (const auto& v : impl_->views) s.views += v.alive ? 1u : 0u;
  VmaTotalStatistics stats{};
  vmaCalculateStatistics(impl_->device->allocator(), &stats);
  s.vma_allocations = stats.total.statistics.allocationCount;
  s.vma_bytes = stats.total.statistics.allocationBytes;
  s.pending_retirements = impl_->device->pending_retirements();
  return s;
}

const FrameStats& Renderer::last_frame() const { return impl_->stats; }

// --- views -----------------------------------------------------------------------------------

View* Renderer::Impl::Find(ViewHandle h) {
  if (!h.valid() || h.index >= views.size()) return nullptr;
  View& v = views[h.index];
  return (v.alive && v.generation == h.generation) ? &v : nullptr;
}

ViewHandle Renderer::CreateView(std::uint32_t width, std::uint32_t height) {
  Impl& im = *impl_;
  if (im.frame_open) {
    LogError("renderer", "CreateView called while a frame is open; ignored");
    return {};
  }
  width = std::max(width, 1u);
  height = std::max(height, 1u);
  std::uint32_t index = 0;
  while (index < im.views.size() && im.views[index].alive) ++index;
  if (index == im.views.size()) im.views.emplace_back();
  View& v = im.views[index];
  View fresh;
  fresh.generation = v.generation + 1;
  if (fresh.generation == 0) fresh.generation = 1;
  if (auto r = im.CreateTargets(fresh.targets, {width, height}); !r) {
    LogError("renderer", "CreateView failed: {}", r.error().Describe());
    return {};
  }
  if (auto r = im.CreateViewOutput(fresh, {width, height}); !r) {
    LogError("renderer", "CreateView failed: {}", r.error().Describe());
    im.RetireTargets(fresh.targets);
    return {};
  }
  fresh.alive = true;
  fresh.image_generation = v.image_generation + 1;
  v = std::move(fresh);
  return {index, v.generation};
}

bool Renderer::ResizeView(ViewHandle h, std::uint32_t width, std::uint32_t height) {
  Impl& im = *impl_;
  View* v = im.Find(h);
  if (!v) return false;
  if (im.frame_open) {
    // Retiring images referenced by the frame being recorded would be unsafe (immediate uploads
    // advance the timeline before this frame is submitted). Resize between frames.
    LogError("renderer", "ResizeView called while a frame is open; ignored");
    return false;
  }
  width = std::max(width, 1u);
  height = std::max(height, 1u);
  if (v->targets.extent.width == width && v->targets.extent.height == height) return true;
  im.RetireTargets(v->targets);
  vk::RetireImage(*im.device, v->output);
  v->drawn = false;
  if (!im.CreateTargets(v->targets, {width, height}) || !im.CreateViewOutput(*v, {width, height})) {
    LogError("renderer", "ResizeView failed to recreate targets");
    return false;
  }
  ++v->image_generation;
  return true;
}

void Renderer::DestroyView(ViewHandle h) {
  Impl& im = *impl_;
  View* v = im.Find(h);
  if (!v) return;
  if (im.frame_open) {
    LogError("renderer", "DestroyView called while a frame is open; ignored");
    return;
  }
  im.RetireTargets(v->targets);
  vk::RetireImage(*im.device, v->output);
  v->alive = false;
  v->drawn = false;
}

bool Renderer::IsValid(ViewHandle h) const { return impl_->Find(h) != nullptr; }

std::array<std::uint32_t, 2> Renderer::ViewSize(ViewHandle h) const {
  const View* v = impl_->Find(h);
  return v ? std::array<std::uint32_t, 2>{v->targets.extent.width, v->targets.extent.height}
           : std::array<std::uint32_t, 2>{0, 0};
}

std::uint64_t Renderer::ViewImageGeneration(ViewHandle h) const {
  const View* v = impl_->Find(h);
  return v ? v->image_generation : 0;
}

// --- frames ----------------------------------------------------------------------------------

void Renderer::Impl::ResolvePicks() {
  const std::uint64_t done = device->CompletedValue();
  for (auto it = picks.begin(); it != picks.end();) {
    if (it->recorded && it->submit_value != 0 && it->submit_value <= done) {
      FrameContext& f = frames[it->frame];
      vmaInvalidateAllocation(device->allocator(), f.pick_readback.allocation, 0, VK_WHOLE_SIZE);
      std::uint32_t value = 0;
      std::memcpy(&value, static_cast<const std::uint8_t*>(f.pick_readback.mapped) + it->slot * 4, 4);
      PickResult r{it->x, it->y, value, std::nullopt};
      if (value != 0 && value <= it->table.size()) r.entity = it->table[value - 1];
      completed_picks[it->ticket] = r;
      it = picks.erase(it);
    } else {
      ++it;
    }
  }
}

Result<void> Renderer::Impl::RecreateSwapchain() {
  int w = 0, h = 0;
  window->PixelSize(w, h);
  if (w <= 0 || h <= 0) return MakeError(ErrorCode::InvalidState, "window has zero size");
  if (auto r = swapchain->Recreate(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h)); !r) return r;
  DestroyTargetsNow(window_targets);  // device is idle after Recreate
  if (auto r = CreateTargets(window_targets, swapchain->extent()); !r) return r;
  needs_recreate = false;
  description.swapchain = std::format("{}x{} format {} images {} present mode {}", swapchain->extent().width,
                                      swapchain->extent().height, static_cast<int>(swapchain->format()),
                                      swapchain->image_count(), static_cast<int>(swapchain->present_mode()));
  return Ok();
}

bool Renderer::BeginFrame() {
  Impl& im = *impl_;
  if (im.frame_open) {
    LogError("renderer", "BeginFrame called twice without EndFrame");
    return false;
  }
  im.frame_start = std::chrono::steady_clock::now();
  im.stats = {};
  im.stats.frame = im.frame_number;
  FrameContext& f = im.frames[im.frame_index];
  if (auto r = im.device->WaitForValue(f.submit_value); !r) {
    LogError("renderer", "{}", r.error().Describe());
    return false;
  }
  im.device->CollectRetired();
  im.ResolvePicks();
  f.draws_used = 0;
  f.picks_used = 0;
  im.window_acquired = false;
  im.window_drawn = false;

  if (im.window) {
    int w = 0, h = 0;
    im.window->PixelSize(w, h);
    if (im.suspended || w <= 0 || h <= 0) {
      im.stats.skipped = true;  // minimized / zero-size: render nothing, keep resources
      return false;
    }
    if (im.needs_recreate || static_cast<std::uint32_t>(w) != im.swapchain->extent().width ||
        static_cast<std::uint32_t>(h) != im.swapchain->extent().height) {
      if (auto r = im.RecreateSwapchain(); !r) {
        LogWarn("renderer", "swapchain recreation deferred: {}", r.error().message);
        im.stats.skipped = true;
        return false;
      }
    }
    const auto status = im.swapchain->Acquire(f.image_available, im.image_index);
    if (status == vk::Swapchain::Status::OutOfDate || status == vk::Swapchain::Status::Error) {
      im.needs_recreate = true;
      im.stats.skipped = true;
      return false;
    }
    if (status == vk::Swapchain::Status::Suboptimal) im.needs_recreate = true;
    im.window_acquired = true;
    // The acquired image's previous contents are discarded; its first use waits on the acquire
    // semaphore at COLOR_ATTACHMENT_OUTPUT, so chain the layout transition from that stage.
    vk::Image& sc = im.swapchain->image(im.image_index);
    sc.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    sc.last_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    sc.last_access = VK_ACCESS_2_NONE;
  }
  vkResetCommandPool(im.device->device(), f.pool, 0);
  VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(f.cmd, &bi);
  im.frame_open = true;
  return true;
}

void Renderer::Impl::DrawScene(Targets& t, vk::Image& output, VkFormat output_format, bool view_output, ViewHandle view,
                               const RenderSnapshot& snap, const ViewCamera& cam, const DrawOptions& opt) {
  FrameContext& f = frames[frame_index];
  if (f.draws_used >= kMaxDrawsPerFrame) {
    LogError("renderer", "more than {} scene draws in one frame; draw skipped", kMaxDrawsPerFrame);
    return;
  }
  const std::uint32_t slot = f.draws_used++;
  const VkExtent2D extent = t.extent;
  const double aspect = static_cast<double>(extent.width) / static_cast<double>(std::max(extent.height, 1u));
  VkCommandBuffer cmd = f.cmd;

  gpu::FrameConstants fc{};
  fc.view_proj = gpu::Pack(cam.Projection(aspect) * cam.ViewMatrix());
  fc.camera_position = gpu::Pack(GetTranslation(cam.world), 1.0);
  if (snap.light) {
    fc.light_direction = gpu::Pack(snap.light->direction, 1.0);
    fc.light_radiance = gpu::Pack(snap.light->radiance, 0.0);
  }
  const auto& env = snap.environment;
  fc.ambient = gpu::Pack(env.ambient_color * env.ambient_intensity, 0.0);
  fc.fog = gpu::Pack(env.fog_color, env.fog_density);
  std::memcpy(static_cast<std::uint8_t*>(f.ubo_arena.mapped) + slot * ubo_stride, &fc, sizeof(fc));

  const std::uint32_t count = static_cast<std::uint32_t>(std::min<std::size_t>(snap.instances.size(), kMaxInstancesPerDraw));
  if (count < snap.instances.size()) {
    LogError("renderer", "snapshot has {} instances; only {} drawn", snap.instances.size(), kMaxInstancesPerDraw);
  }
  auto* inst_out = reinterpret_cast<gpu::InstanceData*>(static_cast<std::uint8_t*>(f.instance_arena.mapped) + slot * instance_stride);
  for (std::uint32_t i = 0; i < count; ++i) {
    const RenderInstance& ri = snap.instances[i];
    gpu::InstanceData& d = inst_out[i];
    d.model = gpu::Pack(ri.model);
    d.normal_matrix = gpu::Pack(ri.normal_matrix);
    d.base_color_roughness = gpu::Pack(ri.material.base_color, ri.material.roughness);
    d.emissive = gpu::Pack(ri.material.emissive * ri.material.emissive_strength, 0.0);
    d.params = {static_cast<float>(ri.material.metallic), 0, 0, 0};
    d.ids[0] = ri.pick_index;
    d.ids[1] = d.ids[2] = d.ids[3] = 0;
  }
  vmaFlushAllocation(device->allocator(), f.ubo_arena.allocation, slot * ubo_stride, sizeof(fc));
  vmaFlushAllocation(device->allocator(), f.instance_arena.allocation, slot * instance_stride,
                     std::max<VkDeviceSize>(1, VkDeviceSize{count} * sizeof(gpu::InstanceData)));

  // Forward pass.
  vk::Transition(cmd, t.hdr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                 VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, true);
  vk::Transition(cmd, t.pick, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                 VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, true);
  vk::Transition(cmd, t.depth, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                 VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                 VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, true);
  VkRenderingAttachmentInfo colors[2] = {{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO},
                                         {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO}};
  colors[0].imageView = t.hdr.view;
  colors[0].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  colors[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colors[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colors[0].clearValue.color = {{static_cast<float>(env.background_color.x), static_cast<float>(env.background_color.y),
                                 static_cast<float>(env.background_color.z), 1.0f}};
  colors[1] = colors[0];
  colors[1].imageView = t.pick.view;
  colors[1].clearValue.color.uint32[0] = 0;
  colors[1].clearValue.color.uint32[1] = colors[1].clearValue.color.uint32[2] = colors[1].clearValue.color.uint32[3] = 0;
  VkRenderingAttachmentInfo depth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  depth.imageView = t.depth.view;
  depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depth.clearValue.depthStencil = {0.0f, 0};  // reversed-Z: far = 0
  VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
  ri.renderArea = {{0, 0}, extent};
  ri.layerCount = 1;
  ri.colorAttachmentCount = 2;
  ri.pColorAttachments = colors;
  ri.pDepthAttachment = &depth;
  vkCmdBeginRendering(cmd, &ri);
  const VkViewport viewport{0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f};
  const VkRect2D scissor{{0, 0}, extent};
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &scissor);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, forward_pipeline);
  const std::uint32_t offsets[2] = {static_cast<std::uint32_t>(slot * ubo_stride), static_cast<std::uint32_t>(slot * instance_stride)};
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, forward_layout, 0, 1, &f.scene_set, 2, offsets);
  const VkDeviceSize vb_offset = 0;
  vkCmdBindVertexBuffers(cmd, 0, 1, &vertices.buffer, &vb_offset);
  vkCmdBindIndexBuffer(cmd, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
  for (std::uint32_t i = 0; i < count; ++i) {
    const ShapeRange& s = shapes[static_cast<int>(snap.instances[i].shape)];
    vkCmdPushConstants(cmd, forward_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4, &i);
    vkCmdDrawIndexed(cmd, s.index_count, 1, s.first_index, s.vertex_offset, 0);
    stats.triangles += s.triangles;
  }
  vkCmdEndRendering(cmd);
  stats.draw_calls += count + 1;
  stats.instances += count;

  // Pick readbacks requested for this view (1 texel each).
  bool host_barrier = false;
  for (auto& p : picks) {
    if (p.recorded || !(p.view == view) || !view.valid()) continue;
    if (f.picks_used >= kMaxPicksPerFrame) break;
    if (t.pick.layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
      vk::Transition(cmd, t.pick, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT,
                     VK_ACCESS_2_TRANSFER_READ_BIT);
      ReadbackWriteAfterWriteBarrier(cmd);
    }
    const std::uint32_t x = std::min(p.x, extent.width - 1), y = std::min(p.y, extent.height - 1);
    VkBufferImageCopy region{};
    region.bufferOffset = f.picks_used * 4;
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset = {static_cast<std::int32_t>(x), static_cast<std::int32_t>(y), 0};
    region.imageExtent = {1, 1, 1};
    vkCmdCopyImageToBuffer(cmd, t.pick.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, f.pick_readback.buffer, 1, &region);
    p.recorded = true;
    p.frame = frame_index;
    p.slot = f.picks_used++;
    host_barrier = true;
  }
  if (host_barrier) HostReadBarrier(cmd);

  // Tone map (+ selection outline) into the output.
  vk::Transition(cmd, t.hdr, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                 VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
  vk::Transition(cmd, t.pick, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                 VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
  vk::Transition(cmd, output, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                 VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, true);
  VkRenderingAttachmentInfo out{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  out.imageView = output.view;
  out.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  out.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  // every pixel is written
  out.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  VkRenderingInfo ti{VK_STRUCTURE_TYPE_RENDERING_INFO};
  ti.renderArea = {{0, 0}, extent};
  ti.layerCount = 1;
  ti.colorAttachmentCount = 1;
  ti.pColorAttachments = &out;
  auto pipeline = TonemapPipeline(output_format);
  if (!pipeline) {
    LogError("renderer", "{}", pipeline.error().Describe());
    return;
  }
  vkCmdBeginRendering(cmd, &ti);
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &scissor);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, tonemap_layout, 0, 1, &t.tonemap_set, 0, nullptr);
  gpu::TonemapPush push{};
  push.outline_color = gpu::Pack(opt.outline_color, 0.9);
  push.exposure = static_cast<float>(env.exposure);
  push.selected_pick = opt.selected_pick_index;
  push.width = extent.width;
  push.height = extent.height;
  vkCmdPushConstants(cmd, tonemap_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRendering(cmd);
  if (view_output) {
    vk::Transition(cmd, output, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                   VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
  }
}

void Renderer::DrawToView(ViewHandle h, const RenderSnapshot& snapshot, const ViewCamera& camera, const DrawOptions& options) {
  Impl& im = *impl_;
  if (!im.frame_open) return;
  View* v = im.Find(h);
  if (!v) {
    LogWarn("renderer", "DrawToView: stale or invalid view handle");
    return;
  }
  im.DrawScene(v->targets, v->output, kViewOutputFormat, true, h, snapshot, camera, options);
  v->drawn = true;
}

void Renderer::DrawToWindow(const RenderSnapshot& snapshot, const ViewCamera& camera, const DrawOptions& options) {
  Impl& im = *impl_;
  if (!im.frame_open || !im.window_acquired) return;
  vk::Image& sc = im.swapchain->image(im.image_index);
  im.DrawScene(im.window_targets, sc, im.swapchain->format(), false, {}, snapshot, camera, options);
  im.window_drawn = true;
}

void Renderer::EndFrame() {
  Impl& im = *impl_;
  if (!im.frame_open) return;
  FrameContext& f = im.frames[im.frame_index];
  VkCommandBuffer cmd = f.cmd;
  bool captured_this_frame = false;
  if (im.window_acquired) {
    vk::Image& sc = im.swapchain->image(im.image_index);
    if (!im.window_drawn || im.overlay) {
      const bool clear = !im.window_drawn;
      vk::Transition(cmd, sc, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT, clear);
      VkRenderingAttachmentInfo a{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
      a.imageView = sc.view;
      a.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      a.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
      a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      a.clearValue.color = {{0.018f, 0.018f, 0.020f, 1.0f}};
      VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
      ri.renderArea = {{0, 0}, im.swapchain->extent()};
      ri.layerCount = 1;
      ri.colorAttachmentCount = 1;
      ri.pColorAttachments = &a;
      vkCmdBeginRendering(cmd, &ri);
      if (im.overlay) im.overlay(cmd);
      vkCmdEndRendering(cmd);
    }
    if (im.capture_requested && im.swapchain->supports_transfer_src()) {
      const VkExtent2D e = im.swapchain->extent();
      const VkDeviceSize bytes = VkDeviceSize{e.width} * e.height * 4;
      if (f.window_capture.size < bytes) {
        vk::RetireBuffer(*im.device, f.window_capture);
        auto b = vk::CreateBuffer(*im.device, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, vk::MemoryKind::HostReadback);
        if (b) f.window_capture = *b;
      }
      if (f.window_capture.buffer) {
        vk::Transition(cmd, sc, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
        ReadbackWriteAfterWriteBarrier(cmd);
        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {e.width, e.height, 1};
        vkCmdCopyImageToBuffer(cmd, sc.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, f.window_capture.buffer, 1, &region);
        HostReadBarrier(cmd);
        captured_this_frame = true;
        im.capture_extent = e;
        im.capture_format = im.swapchain->format();
      }
      im.capture_requested = false;
    }
    // The layout transition is a write: chain it into the present semaphore's signal scope (the
    // signal uses ALL_COMMANDS) by naming a destination stage inside that scope. With dst = NONE the
    // transition is not ordered before the signal, and present can observe it unfinished.
    vk::Transition(cmd, sc, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_NONE);
  }
  vkEndCommandBuffer(cmd);

  auto value = im.window_acquired
                   ? im.device->Submit(cmd, f.image_available, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                       im.swapchain->render_finished(im.image_index))
                   : im.device->Submit(cmd, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
  if (!value) {
    LogError("renderer", "frame submission failed: {}", value.error().Describe());
  } else {
    f.submit_value = *value;
    for (auto& p : im.picks) {
      if (p.recorded && p.submit_value == 0 && p.frame == im.frame_index) p.submit_value = *value;
    }
    if (captured_this_frame) {
      im.capture_pending = true;
      im.capture_value = *value;
      im.capture_frame = im.frame_index;
    }
    if (im.window_acquired) {
      const auto status = im.swapchain->Present(im.image_index);
      if (status != vk::Swapchain::Status::Ok) im.needs_recreate = true;
    }
  }
  im.stats.cpu_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - im.frame_start).count();
  im.frame_open = false;
  im.window_acquired = false;
  im.frame_index = (im.frame_index + 1) % kFramesInFlight;
  ++im.frame_number;
}

void Renderer::NotifyWindowResized() { impl_->needs_recreate = true; }
void Renderer::SetPresentationSuspended(bool s) { impl_->suspended = s; }
bool Renderer::presentation_suspended() const { return impl_->suspended; }

// --- readback --------------------------------------------------------------------------------

std::uint64_t Renderer::RequestPick(ViewHandle view, std::uint32_t x, std::uint32_t y, std::vector<Uuid> pick_table) {
  Impl& im = *impl_;
  PendingPick p;
  p.ticket = im.next_ticket++;
  p.view = view;
  p.x = x;
  p.y = y;
  p.table = std::move(pick_table);
  im.picks.push_back(std::move(p));
  return im.picks.back().ticket;
}

std::optional<PickResult> Renderer::TakePick(std::uint64_t ticket) {
  Impl& im = *impl_;
  im.ResolvePicks();
  auto it = im.completed_picks.find(ticket);
  if (it == im.completed_picks.end()) return std::nullopt;
  PickResult r = it->second;
  im.completed_picks.erase(it);
  return r;
}

Result<vk::Buffer> Renderer::Impl::ReadbackImage(vk::Image& img, VkImageAspectFlags aspect, VkDeviceSize bytes,
                                                 VkImageLayout restore, VkPipelineStageFlags2 restore_stage,
                                                 VkAccessFlags2 restore_access) {
  if (frame_open) return MakeError(ErrorCode::InvalidState, "capture requested while a frame is being recorded");
  device->WaitIdle();
  auto buffer = vk::CreateBuffer(*device, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, vk::MemoryKind::HostReadback);
  if (!buffer) return buffer.take_error();
  const VkBuffer dst = buffer->buffer;
  auto r = device->ImmediateSubmit([&](VkCommandBuffer cmd) {
    vk::Transition(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
    VkBufferImageCopy region{};
    region.imageSubresource = {aspect, 0, 0, 1};
    region.imageExtent = {img.extent.width, img.extent.height, 1};
    vkCmdCopyImageToBuffer(cmd, img.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst, 1, &region);
    HostReadBarrier(cmd);
    vk::Transition(cmd, img, restore, restore_stage, restore_access);
  });
  if (!r) {
    vk::DestroyBuffer(*device, *buffer);
    return r.take_error();
  }
  vmaInvalidateAllocation(device->allocator(), buffer->allocation, 0, VK_WHOLE_SIZE);
  return buffer;
}

Result<ImageRgba8> Renderer::CaptureView(ViewHandle h) {
  Impl& im = *impl_;
  View* v = im.Find(h);
  if (!v) return MakeError(ErrorCode::InvalidValue, "invalid view handle");
  if (!v->drawn) return MakeError(ErrorCode::InvalidState, "view has not been drawn since creation/resize");
  const VkExtent2D e = v->output.extent;
  auto buf = im.ReadbackImage(v->output, VK_IMAGE_ASPECT_COLOR_BIT, VkDeviceSize{e.width} * e.height * 4,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                              VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
  if (!buf) return buf.take_error();
  ImageRgba8 img;
  img.width = e.width;
  img.height = e.height;
  img.pixels.resize(std::size_t(e.width) * e.height * 4);
  std::memcpy(img.pixels.data(), buf->mapped, img.pixels.size());
  vk::DestroyBuffer(*im.device, *buf);
  return img;
}

Result<std::vector<float>> Renderer::CaptureViewDepth(ViewHandle h) {
  Impl& im = *impl_;
  View* v = im.Find(h);
  if (!v || !v->drawn) return MakeError(ErrorCode::InvalidState, "view is invalid or not drawn");
  const VkExtent2D e = v->targets.depth.extent;
  auto buf = im.ReadbackImage(v->targets.depth, VK_IMAGE_ASPECT_DEPTH_BIT, VkDeviceSize{e.width} * e.height * 4,
                              VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                              VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                              VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
  if (!buf) return buf.take_error();
  std::vector<float> depth(std::size_t(e.width) * e.height);
  std::memcpy(depth.data(), buf->mapped, depth.size() * 4);
  vk::DestroyBuffer(*im.device, *buf);
  return depth;
}

Result<std::vector<std::uint32_t>> Renderer::CaptureViewPickIds(ViewHandle h) {
  Impl& im = *impl_;
  View* v = im.Find(h);
  if (!v || !v->drawn) return MakeError(ErrorCode::InvalidState, "view is invalid or not drawn");
  const VkExtent2D e = v->targets.pick.extent;
  auto buf = im.ReadbackImage(v->targets.pick, VK_IMAGE_ASPECT_COLOR_BIT, VkDeviceSize{e.width} * e.height * 4,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                              VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
  if (!buf) return buf.take_error();
  std::vector<std::uint32_t> ids(std::size_t(e.width) * e.height);
  std::memcpy(ids.data(), buf->mapped, ids.size() * 4);
  vk::DestroyBuffer(*im.device, *buf);
  return ids;
}

void Renderer::RequestWindowCapture() { impl_->capture_requested = true; }
bool Renderer::window_capture_supported() const { return impl_->swapchain && impl_->swapchain->supports_transfer_src(); }

Result<ImageRgba8> Renderer::TakeWindowCapture() {
  Impl& im = *impl_;
  if (!im.capture_pending) return MakeError(ErrorCode::InvalidState, "no window capture was recorded");
  if (auto r = im.device->WaitForValue(im.capture_value); !r) return r.take_error();
  FrameContext& f = im.frames[im.capture_frame];
  vmaInvalidateAllocation(im.device->allocator(), f.window_capture.allocation, 0, VK_WHOLE_SIZE);
  ImageRgba8 img;
  img.width = im.capture_extent.width;
  img.height = im.capture_extent.height;
  img.pixels.resize(std::size_t(img.width) * img.height * 4);
  std::memcpy(img.pixels.data(), f.window_capture.mapped, img.pixels.size());
  if (im.capture_format == VK_FORMAT_B8G8R8A8_SRGB || im.capture_format == VK_FORMAT_B8G8R8A8_UNORM) {
    for (std::size_t i = 0; i < img.pixels.size(); i += 4) std::swap(img.pixels[i], img.pixels[i + 2]);
  }
  for (std::size_t i = 3; i < img.pixels.size(); i += 4) img.pixels[i] = 255;  // opaque composite
  im.capture_pending = false;
  return img;
}

Result<std::array<Vec4, 4>> Renderer::RunMatrixPackingProbe(const Mat4& m, const Vec4& v) {
  Impl& im = *impl_;
  vk::Device& d = *im.device;
  if (im.frame_open) return MakeError(ErrorCode::InvalidState, "probe requested while a frame is open");
  gpu::ProbeInput in{gpu::Pack(m), {static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z), static_cast<float>(v.w)}};
  auto ubo = vk::CreateBuffer(d, sizeof(in), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, vk::MemoryKind::HostUpload);
  auto sbo = vk::CreateBuffer(d, sizeof(in), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vk::MemoryKind::HostUpload);
  auto out = vk::CreateBuffer(d, 64, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vk::MemoryKind::HostReadback);
  if (!ubo || !sbo || !out) return MakeError(ErrorCode::IoFailure, "probe buffer allocation failed");
  std::memcpy(ubo->mapped, &in, sizeof(in));
  std::memcpy(sbo->mapped, &in, sizeof(in));
  vmaFlushAllocation(d.allocator(), ubo->allocation, 0, VK_WHOLE_SIZE);
  vmaFlushAllocation(d.allocator(), sbo->allocation, 0, VK_WHOLE_SIZE);
  VkDescriptorSet set = VK_NULL_HANDLE;
  VkDescriptorSetAllocateInfo dai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  dai.descriptorPool = im.pool;
  dai.descriptorSetCount = 1;
  dai.pSetLayouts = &im.probe_set_layout;
  SOV_VK_TRY(vkAllocateDescriptorSets(d.device(), &dai, &set), "probe descriptor set");
  const VkDescriptorBufferInfo infos[3] = {{ubo->buffer, 0, sizeof(in)}, {sbo->buffer, 0, sizeof(in)}, {out->buffer, 0, 64}};
  VkWriteDescriptorSet w[3] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                               {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
  for (std::uint32_t i = 0; i < 3; ++i) {
    w[i].dstSet = set;
    w[i].dstBinding = i;
    w[i].descriptorCount = 1;
    w[i].descriptorType = i == 0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    w[i].pBufferInfo = &infos[i];
  }
  vkUpdateDescriptorSets(d.device(), 3, w, 0, nullptr);
  auto r = d.ImmediateSubmit([&](VkCommandBuffer cmd) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, im.probe_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, im.probe_layout, 0, 1, &set, 0, nullptr);
    vkCmdDispatch(cmd, 1, 1, 1);
    HostReadBarrier(cmd);
  });
  std::array<Vec4, 4> result{};
  if (r) {
    vmaInvalidateAllocation(d.allocator(), out->allocation, 0, VK_WHOLE_SIZE);
    const float* f = static_cast<const float*>(out->mapped);
    for (int i = 0; i < 4; ++i) result[static_cast<std::size_t>(i)] = {f[i * 4], f[i * 4 + 1], f[i * 4 + 2], f[i * 4 + 3]};
  }
  vkFreeDescriptorSets(d.device(), im.pool, 1, &set);
  vk::DestroyBuffer(d, *ubo);
  vk::DestroyBuffer(d, *sbo);
  vk::DestroyBuffer(d, *out);
  if (!r) return r.take_error();
  return result;
}

void Renderer::WaitIdle() { impl_->device->WaitIdle(); }

// --- camera ----------------------------------------------------------------------------------

std::optional<ViewCamera> ViewCamera::FromSnapshot(const RenderSnapshot& snap) {
  if (!snap.active_camera) return std::nullopt;
  return ViewCamera{snap.active_camera->world, snap.active_camera->vertical_fov_radians, snap.active_camera->near_clip,
                    snap.active_camera->far_clip};
}

Mat4 ViewCamera::ViewMatrix() const {
  // World matrices may carry scale; the view uses the camera's rigid placement only.
  const Vec3 x = Normalize(TransformDirection(world, {1, 0, 0}));
  const Vec3 y = Normalize(TransformDirection(world, {0, 1, 0}));
  const Vec3 z = Normalize(TransformDirection(world, {0, 0, 1}));
  const Vec3 p = GetTranslation(world);
  Mat4 v;
  v.at(0, 0) = x.x; v.at(0, 1) = x.y; v.at(0, 2) = x.z; v.at(0, 3) = -Dot(x, p);
  v.at(1, 0) = y.x; v.at(1, 1) = y.y; v.at(1, 2) = y.z; v.at(1, 3) = -Dot(y, p);
  v.at(2, 0) = z.x; v.at(2, 1) = z.y; v.at(2, 2) = z.z; v.at(2, 3) = -Dot(z, p);
  return v;
}

Mat4 ViewCamera::Projection(double aspect) const { return PerspectiveReversedZ(vertical_fov_radians, aspect, near_clip, far_clip); }

PngText CaptureProvenance(const Renderer& renderer, const std::string& source, const std::string& scene) {
  const auto& d = renderer.device();
  const auto v = renderer.validation();
  const auto& b = GetBuildInfo();
  return {{"Software", "Sovereign Engine " + std::string(b.version)},
          {"Source", source},
          {"Scene", scene},
          {"EvidenceClass", d.evidence_class},
          {"Device", d.name + " [" + d.device_type + "]"},
          {"Driver", d.driver},
          {"Backend", "native Vulkan " + d.api_version + " (volk + VMA), engine-owned HLSL->SPIR-V shaders"},
          {"ShaderCompiler", SOV_SHADER_COMPILER_DESC},
          {"ValidationLayer", v.layer_enabled ? (v.synchronization_validation ? "on (+synchronization)" : "on") : "off"},
          {"ValidationErrorsAtCapture", std::to_string(v.errors)},
          {"EngineCommit", std::string(b.git_commit) + " (" + b.git_tree_state + ")"},
          {"CapturedAt", CurrentUtcTimestamp()}};
}

// --- native bridge ---------------------------------------------------------------------------

namespace native {

VulkanObjects Objects(Renderer& r) {
  auto& im = r.impl();
  VulkanObjects o;
  o.instance = im.device->instance();
  o.physical_device = im.device->physical();
  o.device = im.device->device();
  o.queue = im.device->queue();
  o.queue_family = im.device->queue_family();
  if (im.swapchain) {
    o.swapchain_format = im.swapchain->format();
    o.swapchain_min_images = im.swapchain->min_image_count();
    o.swapchain_images = im.swapchain->image_count();
  }
  return o;
}

void SetOverlayRecorder(Renderer& r, std::function<void(VkCommandBuffer)> record) { r.impl().overlay = std::move(record); }

VkImageView ViewOutputImageView(Renderer& r, ViewHandle h) {
  View* v = r.impl().Find(h);
  return v ? v->output.view : VK_NULL_HANDLE;
}

void RetireAfterGpu(Renderer& r, std::function<void()> destroy) { r.impl().device->Retire(std::move(destroy)); }

}  // namespace native

}  // namespace sov::render
