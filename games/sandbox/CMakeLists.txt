# Original sandbox game module (no Vulkan, no editor dependencies).
add_library(sov_game_sandbox STATIC src/sandbox.cpp)
add_library(sov::game_sandbox ALIAS sov_game_sandbox)
sov_configure_target(sov_game_sandbox)
target_include_directories(sov_game_sandbox PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(sov_game_sandbox PUBLIC sov::runtime)
