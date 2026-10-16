add_library(sov_runtime STATIC src/simulation.cpp)
add_library(sov::runtime ALIAS sov_runtime)
sov_configure_target(sov_runtime)
target_include_directories(sov_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(sov_runtime PUBLIC sov::world sov::core)

if(SOV_ENABLE_GRAPHICS)
  # Standalone runtime: world + runtime + game module + renderer + platform. No editor/UI/command code.
  add_executable(sovereign-runtime app/main.cpp)
  sov_configure_target(sovereign-runtime)
  target_link_libraries(sovereign-runtime PRIVATE sov::game_sandbox sov::renderer sov::platform sov::runtime sov::world sov::core)
endif()
