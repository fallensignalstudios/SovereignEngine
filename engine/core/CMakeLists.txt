add_library(sov_core STATIC
  src/fs.cpp
  src/json.cpp
  src/log.cpp
  src/math.cpp
  src/result.cpp
  src/sha256.cpp
  src/uuid.cpp
  src/build_info.cpp)
add_library(sov::core ALIAS sov_core)
sov_configure_target(sov_core)
add_dependencies(sov_core sov_build_info)
target_include_directories(sov_core
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${SOV_GENERATED_DIR}")
target_link_libraries(sov_core PUBLIC nlohmann_json::nlohmann_json)
