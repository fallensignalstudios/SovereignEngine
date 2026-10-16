sov_compile_shaders(SOV_RENDERER_SHADERS
  SOURCES
    "shaders/forward.hlsl|vs|VSMain"
    "shaders/forward.hlsl|ps|PSMain"
    "shaders/tonemap.hlsl|vs|VSMain"
    "shaders/tonemap.hlsl|ps|PSMain"
    "shaders/matrix_probe.hlsl|cs|CSMain"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/common.hlsli")
add_custom_target(sov_shaders DEPENDS ${SOV_RENDERER_SHADERS})

add_library(sov_renderer STATIC src/renderer.cpp)
add_library(sov::renderer ALIAS sov_renderer)
sov_configure_target(sov_renderer)
add_dependencies(sov_renderer sov_shaders)
target_include_directories(sov_renderer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(sov_renderer PUBLIC sov::world sov::core sov::rhi_vulkan PRIVATE sov::platform)
