add_library(sov_world STATIC
  src/primitive_geometry.cpp
  src/reflection.cpp
  src/scene_diff.cpp
  src/scene_document.cpp
  src/world.cpp)
add_library(sov::world ALIAS sov_world)
sov_configure_target(sov_world)
target_include_directories(sov_world PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(sov_world
  PUBLIC sov::core
  PRIVATE sov::entt)   # EnTT is an implementation detail of sov::World
