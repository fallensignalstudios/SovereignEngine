#include "imgui_layer.h"

#include <cmath>

#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include "sov/core/log.h"
#include "sov/platform/window.h"
#include "sov/renderer/vulkan_native.h"

namespace sov::editor_app {
namespace {

void CheckVk(VkResult r) {
  if (r != VK_SUCCESS) LogError("imgui", "Vulkan backend call failed: {}", static_cast<int>(r));
}

std::uint64_t Key(render::ViewHandle v) { return (std::uint64_t{v.index} << 32) | v.generation; }

// ImGui colors are authored in sRGB; the swapchain applies the sRGB encode on store, so the
// palette is converted to linear once here to display as authored.
ImVec4 Srgb(float r, float g, float b, float a = 1.0f) {
  const auto lin = [](float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); };
  return ImVec4(lin(r), lin(g), lin(b), a);
}

}  // namespace

void ApplyWorkbenchTheme() {
  ImGuiStyle& s = ImGui::GetStyle();
  ImGui::StyleColorsDark(&s);
  s.WindowRounding = 2.0f;
  s.FrameRounding = 2.0f;
  s.TabRounding = 2.0f;
  s.GrabRounding = 2.0f;
  s.WindowBorderSize = 1.0f;
  s.FramePadding = ImVec2(6, 4);
  s.ItemSpacing = ImVec2(8, 5);
  s.FontScaleMain = 1.15f;
  ImVec4* c = s.Colors;
  const ImVec4 graphite0 = Srgb(0.075f, 0.078f, 0.085f), graphite1 = Srgb(0.11f, 0.115f, 0.125f),
               graphite2 = Srgb(0.16f, 0.165f, 0.18f), graphite3 = Srgb(0.22f, 0.225f, 0.245f),
               bronze = Srgb(0.72f, 0.50f, 0.28f), bronze_dim = Srgb(0.45f, 0.32f, 0.19f),
               text = Srgb(0.88f, 0.87f, 0.85f), text_dim = Srgb(0.55f, 0.55f, 0.56f);
  c[ImGuiCol_Text] = text;
  c[ImGuiCol_TextDisabled] = text_dim;
  c[ImGuiCol_WindowBg] = graphite1;
  c[ImGuiCol_ChildBg] = graphite1;
  c[ImGuiCol_PopupBg] = graphite1;
  c[ImGuiCol_Border] = graphite3;
  c[ImGuiCol_FrameBg] = graphite0;
  c[ImGuiCol_FrameBgHovered] = graphite2;
  c[ImGuiCol_FrameBgActive] = graphite3;
  c[ImGuiCol_TitleBg] = graphite0;
  c[ImGuiCol_TitleBgActive] = graphite2;
  c[ImGuiCol_MenuBarBg] = graphite0;
  c[ImGuiCol_Header] = Srgb(0.45f, 0.32f, 0.19f, 0.55f);
  c[ImGuiCol_HeaderHovered] = Srgb(0.55f, 0.39f, 0.23f, 0.70f);
  c[ImGuiCol_HeaderActive] = bronze_dim;
  c[ImGuiCol_Button] = graphite2;
  c[ImGuiCol_ButtonHovered] = graphite3;
  c[ImGuiCol_ButtonActive] = bronze_dim;
  c[ImGuiCol_CheckMark] = bronze;
  c[ImGuiCol_SliderGrab] = bronze_dim;
  c[ImGuiCol_SliderGrabActive] = bronze;
  c[ImGuiCol_Tab] = graphite0;
  c[ImGuiCol_TabHovered] = graphite3;
  c[ImGuiCol_TabSelected] = graphite2;
  c[ImGuiCol_TabSelectedOverline] = bronze;
  c[ImGuiCol_TabDimmed] = graphite0;
  c[ImGuiCol_TabDimmedSelected] = graphite1;
  c[ImGuiCol_DockingPreview] = Srgb(0.72f, 0.50f, 0.28f, 0.6f);
  c[ImGuiCol_DockingEmptyBg] = graphite0;
  c[ImGuiCol_SeparatorHovered] = bronze_dim;
  c[ImGuiCol_SeparatorActive] = bronze;
  c[ImGuiCol_ResizeGrip] = graphite3;
  c[ImGuiCol_ResizeGripHovered] = bronze_dim;
  c[ImGuiCol_ResizeGripActive] = bronze;
  c[ImGuiCol_NavCursor] = bronze;
}

Result<void> ImGuiLayer::Init(platform::Window& window, render::Renderer& renderer) {
  renderer_ = &renderer;
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = nullptr;  // deterministic default layout; no editor-local files written
  ApplyWorkbenchTheme();
  if (!ImGui_ImplSDL3_InitForVulkan(window.sdl())) {
    return MakeError(ErrorCode::IoFailure, "ImGui_ImplSDL3_InitForVulkan failed");
  }
  const auto o = render::native::Objects(renderer);
  ImGui_ImplVulkan_InitInfo info{};
  info.ApiVersion = VK_API_VERSION_1_3;
  info.Instance = o.instance;
  info.PhysicalDevice = o.physical_device;
  info.Device = o.device;
  info.QueueFamily = o.queue_family;
  info.Queue = o.queue;
  info.DescriptorPoolSize = 64;  // backend-owned pool: font atlas + viewport textures
  info.MinImageCount = o.swapchain_min_images < 2 ? 2 : o.swapchain_min_images;
  info.ImageCount = o.swapchain_images < info.MinImageCount ? info.MinImageCount : o.swapchain_images;
  info.UseDynamicRendering = true;
  info.PipelineInfoMain.PipelineRenderingCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
  info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &o.swapchain_format;
  info.CheckVkResultFn = CheckVk;
  if (!ImGui_ImplVulkan_Init(&info)) return MakeError(ErrorCode::IoFailure, "ImGui_ImplVulkan_Init failed");
  min_images_ = info.MinImageCount;
  render::native::SetOverlayRecorder(renderer, [](VkCommandBuffer cmd) {
    if (ImDrawData* dd = ImGui::GetDrawData()) ImGui_ImplVulkan_RenderDrawData(dd, cmd);
  });
  initialized_ = true;
  return Ok();
}

void ImGuiLayer::Shutdown() {
  if (!initialized_) return;
  renderer_->WaitIdle();
  render::native::SetOverlayRecorder(*renderer_, nullptr);
  for (auto& [key, reg] : textures_) ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(reg.descriptor));
  textures_.clear();
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  initialized_ = false;
}

void ImGuiLayer::ProcessEvent(const SDL_Event& e) { ImGui_ImplSDL3_ProcessEvent(&e); }

void ImGuiLayer::NewFrame() {
  const auto o = render::native::Objects(*renderer_);
  if (o.swapchain_min_images >= 2 && o.swapchain_min_images != min_images_) {
    min_images_ = o.swapchain_min_images;
    ImGui_ImplVulkan_SetMinImageCount(min_images_);
  }
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
}

void ImGuiLayer::Render() { ImGui::Render(); }

ImTextureID ImGuiLayer::ViewTexture(render::ViewHandle view) {
  if (!renderer_->IsValid(view)) return ImTextureID{};
  const std::uint64_t gen = renderer_->ViewImageGeneration(view);
  Registered& reg = textures_[Key(view)];
  if (reg.descriptor == 0 || reg.image_generation != gen) {
    if (reg.descriptor != 0) {
      const auto old = reinterpret_cast<VkDescriptorSet>(reg.descriptor);
      render::native::RetireAfterGpu(*renderer_, [old] { ImGui_ImplVulkan_RemoveTexture(old); });
    }
    const VkImageView iv = render::native::ViewOutputImageView(*renderer_, view);
    reg.descriptor = reinterpret_cast<std::uint64_t>(ImGui_ImplVulkan_AddTexture(iv, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    reg.image_generation = gen;
  }
  return static_cast<ImTextureID>(reg.descriptor);
}

}  // namespace sov::editor_app
