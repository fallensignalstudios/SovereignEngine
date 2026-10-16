add_library(sov_commands STATIC
  src/command_batch.cpp
  src/operation.cpp
  src/scene_session.cpp
  src/scene_store.cpp
  src/transaction.cpp)
add_library(sov::commands ALIAS sov_commands)
sov_configure_target(sov_commands)
target_include_directories(sov_commands PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(sov_commands PUBLIC sov::world sov::core)
