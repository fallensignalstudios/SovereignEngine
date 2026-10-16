# Authoring services: shared by the editor GUI and headless tests. No UI library dependency.
add_library(sov_editor_authoring STATIC authoring/src/editor_document.cpp)
add_library(sov::editor_authoring ALIAS sov_editor_authoring)
sov_configure_target(sov_editor_authoring)
target_include_directories(sov_editor_authoring PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/authoring/include")
target_link_libraries(sov_editor_authoring PUBLIC sov::commands sov::runtime sov::world sov::core)

if(SOV_ENABLE_GRAPHICS)
  add_subdirectory(application)
endif()
