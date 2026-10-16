#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "sov/core/math.h"
#include "sov/core/png.h"
#include "sov/core/result.h"
#include "sov/core/uuid.h"
#include "sov/world/render_snapshot.h"

namespace sov::platform {
class Window;
}

// Renderer R0 (TDD §10.2): forward pass of original primitives into linear HDR with reversed-Z
// depth and an entity-ID target, then tone mapping to an sRGB output. One Vulkan 1.3 graphics
// queue, a two-context frame ring, timeline-semaphore completion, deferred resource retirement.
// This public header contains no Vulkan types.
namespace sov::render {

// Generation-checked handle to an offscreen render view (editor viewport, captures, tests).
struct ViewHandle {
  std::uint32_t index = 0;
  std::uint32_t generation = 0;  // 0 = null
  bool valid() const { return generation != 0; }
  bool operator==(const ViewHandle&) const = default;
};

// Camera for one draw. `world` is camera-to-world; the camera looks along its local -Z.
struct ViewCamera {
  Mat4 world;
  double vertical_fov_radians = 1.0;
  double near_clip = 0.1;
  double far_clip = 1000.0;

  static std::optional<ViewCamera> FromSnapshot(const RenderSnapshot& snap);
  Mat4 ViewMatrix() const;
  Mat4 Projection(double aspect) const;
};

struct DrawOptions {
  std::uint32_t selected_pick_index = 0;          // outline this pick index (0 = none)
  Vec3 outline_color{1.0, 0.62, 0.22};            // linear; restrained bronze highlight
};

struct DeviceDescription {
  std::string name;
  std::string device_type;        // "discrete-gpu", "integrated-gpu", "cpu", ...
  bool is_cpu_implementation = false;
  std::string api_version;
  std::string driver;             // driver name + info
  std::string conformance_version;
  std::string instance_version;
  std::vector<std::string> layers;
  std::vector<std::string> requested_features;
  std::vector<std::string> candidates;
  bool windowed = false;
  std::string swapchain;          // format/extent/present mode summary when windowed
  bool gpu_timestamps = false;    // graphics-queue timestamp queries available (FrameStats::gpu_ms)
  // Evidence class of anything this renderer produces (TDD §1.3 / handoff):
  //   "gpu-rendered (physical GPU)" or "software-rendered (CPU Vulkan implementation)"
  std::string evidence_class;
};

struct ValidationSummary {
  bool layer_enabled = false;
  bool synchronization_validation = false;
  std::uint32_t errors = 0;
  std::uint32_t warnings = 0;
  std::vector<std::string> messages;
};

struct ResourceSummary {
  std::int64_t buffers = 0;
  std::int64_t images = 0;
  std::int64_t image_views = 0;
  std::uint32_t views = 0;
  std::uint64_t vma_allocations = 0;
  std::uint64_t vma_bytes = 0;
  std::size_t pending_retirements = 0;
};

struct FrameStats {
  std::uint64_t frame = 0;
  std::uint32_t draw_calls = 0;
  std::uint32_t instances = 0;
  std::uint64_t triangles = 0;
  // Wall time from BeginFrame to the end of EndFrame on the calling thread, and the part of it
  // spent blocked: frame-context fence wait, swapchain acquire and present. cpu_ms - wait_ms is
  // the renderer's own CPU work (snapshot upload, command recording, submission).
  double cpu_ms = 0;
  double wait_ms = 0;
  bool skipped = false;
  // GPU execution time of an earlier frame (`gpu_frame`), from timestamp queries read when that
  // frame's context is reused (no stall). -1 when timestamps are unsupported or not yet available.
  double gpu_ms = -1;
  std::uint64_t gpu_frame = 0;
};

struct PickResult {
  std::uint32_t x = 0, y = 0;
  std::uint32_t pick_index = 0;
  std::optional<Uuid> entity;  // resolved with the pick table of the frame that was read
  std::uint64_t frame = 0;     // renderer frame number of that draw
};

struct RendererConfig {
  bool validation = true;
  bool require_physical_gpu = false;  // also forced by environment SOV_REQUIRE_PHYSICAL_GPU=1
  bool require_validation = false;    // also forced by SOV_REQUIRE_VALIDATION=1 (set for every Vulkan test)
  std::string device_name;            // select by name substring; also SOV_VULKAN_DEVICE
  bool vsync = true;
  std::filesystem::path shader_dir;   // default: <executable dir>/shaders
};

class Renderer {
 public:
  static Result<std::unique_ptr<Renderer>> CreateForWindow(const RendererConfig& config, platform::Window& window);
  static Result<std::unique_ptr<Renderer>> CreateOffscreen(const RendererConfig& config);
  ~Renderer();

  const DeviceDescription& device() const;
  ValidationSummary validation() const;
  ResourceSummary resources() const;
  const FrameStats& last_frame() const;

  // --- offscreen views --------------------------------------------------------------------
  ViewHandle CreateView(std::uint32_t width, std::uint32_t height);
  bool ResizeView(ViewHandle view, std::uint32_t width, std::uint32_t height);  // old targets retired safely
  void DestroyView(ViewHandle view);
  bool IsValid(ViewHandle view) const;
  std::array<std::uint32_t, 2> ViewSize(ViewHandle view) const;
  // Increments whenever the view's output image is replaced (overlay bridges re-register).
  std::uint64_t ViewImageGeneration(ViewHandle view) const;

  // --- frames -----------------------------------------------------------------------------
  // Returns false when the frame must be skipped (zero-size/minimized window, swapchain rebuilt).
  bool BeginFrame();
  void DrawToView(ViewHandle view, const RenderSnapshot& snapshot, const ViewCamera& camera,
                  const DrawOptions& options = {});
  void DrawToWindow(const RenderSnapshot& snapshot, const ViewCamera& camera, const DrawOptions& options = {});
  void EndFrame();
  void NotifyWindowResized();
  void SetPresentationSuspended(bool suspended);  // e.g. on minimize events
  bool presentation_suspended() const;

  // --- readback ---------------------------------------------------------------------------
  // Queues a read of the view's pick-ID texel at (x, y) during its next draw; the result is
  // available once that frame completes. The index is resolved with the pick table of the snapshot
  // that draw actually rendered (bound when the copy is recorded), so a pick can never be mapped
  // through another frame's table.
  std::uint64_t RequestPick(ViewHandle view, std::uint32_t x, std::uint32_t y);
  std::optional<PickResult> TakePick(std::uint64_t ticket);
  // Blocking captures of a view's most recent draw.
  Result<ImageRgba8> CaptureView(ViewHandle view);
  Result<std::vector<float>> CaptureViewDepth(ViewHandle view);
  Result<std::vector<std::uint32_t>> CaptureViewPickIds(ViewHandle view);
  // Captures the next presented window frame (including overlays) after EndFrame.
  void RequestWindowCapture();
  Result<ImageRgba8> TakeWindowCapture();  // blocks until the requested frame completes
  bool window_capture_supported() const;

  // Diagnostic compute probe: returns {cb M*v, sb M*v, cb row0, sb row0} computed on the GPU.
  Result<std::array<Vec4, 4>> RunMatrixPackingProbe(const Mat4& m, const Vec4& v);

  void WaitIdle();

  // Backend-specific access for editor overlays (see renderer/vulkan_native.h).
  struct Impl;
  Impl& impl() { return *impl_; }

 private:
  Renderer() = default;
  std::unique_ptr<Impl> impl_;
};

// Standard provenance metadata for evidence images (embedded as PNG text chunks).
PngText CaptureProvenance(const Renderer& renderer, const std::string& source, const std::string& scene);

}  // namespace sov::render
