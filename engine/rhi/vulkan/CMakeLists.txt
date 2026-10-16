# Native Vulkan 1.3 backend (the only graphics backend; see docs/decisions/ADR-002).
add_library(sov_rhi_vulkan STATIC
  src/device.cpp
  src/resources.cpp
  src/swapchain.cpp
  src/vma_impl.cpp)
add_library(sov::rhi_vulkan ALIAS sov_rhi_vulkan)
sov_configure_target(sov_rhi_vulkan)
# Third-party implementation TU: keep its warnings out of -Werror.
set_source_files_properties(src/vma_impl.cpp PROPERTIES COMPILE_OPTIONS "-w")
target_include_directories(sov_rhi_vulkan PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(sov_rhi_vulkan PUBLIC sov::volk sov::vma sov::core)
