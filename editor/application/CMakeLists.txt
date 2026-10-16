add_executable(sovereign-editor
  main.cpp
  automation.cpp
  editor_app.cpp
  imgui_layer.cpp
  ../panels/panels.cpp)
sov_configure_target(sovereign-editor)
target_link_libraries(sovereign-editor PRIVATE
  sov::editor_authoring sov::game_sandbox sov::renderer sov::platform sov::imgui sov::world sov::core)
