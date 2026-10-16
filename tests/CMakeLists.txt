# Headless native tests: one executable, one CTest entry per suite.
add_library(sov_test_harness STATIC support/sov_test.cpp)
sov_configure_target(sov_test_harness)
target_include_directories(sov_test_harness PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/support")
target_compile_definitions(sov_test_harness PRIVATE
  SOV_TEST_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  SOV_TEST_TEMP_ROOT="${CMAKE_BINARY_DIR}/test-tmp")

set(SOV_HEADLESS_TEST_SOURCES
  unit/test_core.cpp
  unit/test_math.cpp
  unit/test_scene.cpp
  unit/test_world.cpp
  unit/test_commands.cpp
  unit/test_store.cpp
  integration/test_cli.cpp
  integration/test_editor.cpp
  integration/test_play.cpp)
set(SOV_HEADLESS_SUITES core math scene world commands store cli editor play)

add_executable(sovereign-tests support/test_main.cpp ${SOV_HEADLESS_TEST_SOURCES})
sov_configure_target(sovereign-tests)
target_link_libraries(sovereign-tests PRIVATE sov_test_harness sov::core sov::world sov::commands sov::runtime sov::game_sandbox sov::editor_authoring)

target_compile_definitions(sovereign-tests PRIVATE SOV_CLI_PATH="$<TARGET_FILE:sovereign-cli>")
add_dependencies(sovereign-tests sovereign-cli)

foreach(suite IN LISTS SOV_HEADLESS_SUITES)
  add_test(NAME headless.${suite} COMMAND sovereign-tests --suite ${suite})
  set_tests_properties(headless.${suite} PROPERTIES LABELS "headless")
endforeach()
