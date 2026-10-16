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

# --- Vulkan rendering tests (graphics configuration only) -----------------------------------
if(SOV_ENABLE_GRAPHICS)
  add_executable(sovereign-render-tests support/test_main.cpp rendering/test_rendering.cpp)
  sov_configure_target(sovereign-render-tests)
  target_link_libraries(sovereign-render-tests PRIVATE sov_test_harness sov::renderer sov::world sov::core)
  add_test(NAME vulkan.rendering COMMAND sovereign-render-tests --suite rendering)
  # Every Vulkan test requires the Khronos validation layer: a host without it fails the test with
  # a diagnostic instead of silently running unvalidated (G1-R05, G0-07).
  set_tests_properties(vulkan.rendering PROPERTIES LABELS "vulkan" ENVIRONMENT "SOV_REQUIRE_VALIDATION=1")
endif()

# --- static checks on build products and sources (no GPU needed) ----------------------------
add_test(NAME static.no_unreal_dependency
  COMMAND "${CMAKE_COMMAND}" -DSOURCE_DIR=${PROJECT_SOURCE_DIR} -P "${PROJECT_SOURCE_DIR}/cmake/CheckNoUnrealDependency.cmake")
set_tests_properties(static.no_unreal_dependency PROPERTIES LABELS "headless;static")
if(NOT SOV_ENABLE_GRAPHICS AND NOT WIN32)
  # dev-headless binaries must not require a graphics device, SDL or the Vulkan loader.
  foreach(bin sovereign-cli sovereign-tests)
    add_test(NAME static.headless_${bin}_has_no_graphics
      COMMAND "${CMAKE_COMMAND}" -DBINARY=$<TARGET_FILE:${bin}>
        "-DFORBIDDEN=SDL_;vkCreate;volk;ImGui" "-DFORBIDDEN_LIBS=vulkan;SDL;libX11"
        -P "${PROJECT_SOURCE_DIR}/cmake/CheckBinarySymbols.cmake")
    set_tests_properties(static.headless_${bin}_has_no_graphics PROPERTIES LABELS "headless;static")
  endforeach()
endif()

if(SOV_ENABLE_GRAPHICS)
  if(NOT WIN32)
    # The runtime executable links no editor, tool-UI or authoring-command code.
    add_test(NAME static.runtime_has_no_editor_code
      COMMAND "${CMAKE_COMMAND}" -DBINARY=$<TARGET_FILE:sovereign-runtime>
        "-DFORBIDDEN=ImGui;sov::editor;sov::cmd::SceneSession;sov::cmd::SceneStore;EditorDocument"
        -P "${PROJECT_SOURCE_DIR}/cmake/CheckBinarySymbols.cmake")
    set_tests_properties(static.runtime_has_no_editor_code PROPERTIES LABELS "static;vulkan")
  endif()

  # --- windowed tests: real display, or a private Xvfb+openbox display, else SKIPPED --------
  set(launcher "${PROJECT_SOURCE_DIR}/scripts/ctest_display_launcher.sh")
  set(artifacts "${CMAKE_BINARY_DIR}/test-artifacts")
  file(MAKE_DIRECTORY "${artifacts}")
  add_test(NAME vulkan.runtime_first_light
    COMMAND "${launcher}" $<TARGET_FILE:sovereign-runtime> --scene ${PROJECT_SOURCE_DIR}/content/authoring/first_light.scene.json
      --width 1280 --height 720 --frames 120 --scripted-input --screenshot ${artifacts}/runtime_first_light.png)
  add_test(NAME vulkan.runtime_window_lifecycle_stress
    COMMAND "${launcher}" $<TARGET_FILE:sovereign-runtime> --scene ${PROJECT_SOURCE_DIR}/content/authoring/first_light.scene.json
      --width 1280 --height 720 --frames 500 --scripted-input --stress-window)
  add_test(NAME vulkan.editor_first_success_automation
    COMMAND "${CMAKE_COMMAND}" -E chdir ${artifacts}/editor "${launcher}" $<TARGET_FILE:sovereign-editor>
      --scene first_light.scene.json --automation ${PROJECT_SOURCE_DIR}/tests/editor/first_success.automation.json)
  add_test(NAME vulkan.editor_stage_scene
    COMMAND "${CMAKE_COMMAND}" -E copy ${PROJECT_SOURCE_DIR}/content/authoring/first_light.scene.json ${artifacts}/editor/first_light.scene.json)
  add_test(NAME vulkan.editor_clean_stage
    COMMAND "${CMAKE_COMMAND}" -E rm -rf ${artifacts}/editor)
  add_test(NAME vulkan.editor_real_x11_input
    COMMAND "${launcher}" ${PROJECT_SOURCE_DIR}/scripts/gui_real_input_check.sh ${CMAKE_BINARY_DIR}/bin ${artifacts}/gui_real_input)
  # G1-R14 / G0-07: normal close, failed and partial initialization (no display, no driver, missing
  # layer, absent device, missing shaders after device creation) exit with a diagnostic, no hang.
  add_test(NAME vulkan.failure_paths_unwind_cleanly
    COMMAND "${launcher}" ${PROJECT_SOURCE_DIR}/scripts/failure_paths_check.sh ${CMAKE_BINARY_DIR}/bin ${artifacts}/failure_paths)
  set_tests_properties(vulkan.editor_clean_stage PROPERTIES FIXTURES_SETUP editor_clean LABELS "vulkan;display")
  set_tests_properties(vulkan.editor_stage_scene PROPERTIES FIXTURES_SETUP editor_scene FIXTURES_REQUIRED editor_clean LABELS "vulkan;display")
  set_tests_properties(vulkan.editor_first_success_automation PROPERTIES FIXTURES_REQUIRED editor_scene)
  foreach(t vulkan.runtime_first_light vulkan.runtime_window_lifecycle_stress vulkan.editor_first_success_automation
            vulkan.editor_real_x11_input vulkan.failure_paths_unwind_cleanly)
    set_tests_properties(${t} PROPERTIES LABELS "vulkan;display" SKIP_RETURN_CODE 77 TIMEOUT 900
      ENVIRONMENT "SOV_REQUIRE_VALIDATION=1")
  endforeach()
  set_tests_properties(vulkan.editor_real_x11_input PROPERTIES
    ENVIRONMENT "SOV_REQUIRE_VALIDATION=1;SOV_CLI=$<TARGET_FILE:sovereign-cli>")
endif()

# G0-06: executables link only the C/C++ runtime, embed no search paths, name no Unreal types.
if(NOT WIN32)
  set(sov_linked_executables sovereign-cli sovereign-tests)
  if(SOV_ENABLE_GRAPHICS)
    list(APPEND sov_linked_executables sovereign-runtime sovereign-editor sovereign-render-tests)
  endif()
  foreach(bin IN LISTS sov_linked_executables)
    add_test(NAME static.${bin}_link_dependencies
      COMMAND "${CMAKE_COMMAND}" -DBINARY=$<TARGET_FILE:${bin}> "-DSANITIZED=$<BOOL:${SOV_SANITIZE}>"
        -P "${PROJECT_SOURCE_DIR}/cmake/CheckLinkedLibraries.cmake")
    set_tests_properties(static.${bin}_link_dependencies PROPERTIES LABELS "headless;static")
  endforeach()
  if(SOV_ENABLE_GRAPHICS)
    # ...and at run time every loaded object (dlopen'ed Vulkan loader, ICD, layers, X11) is a system library.
    add_test(NAME vulkan.loaded_libraries_are_system_only
      COMMAND "${PROJECT_SOURCE_DIR}/scripts/ctest_display_launcher.sh" ${PROJECT_SOURCE_DIR}/scripts/loaded_libraries_check.sh
        ${CMAKE_BINARY_DIR}/bin ${CMAKE_BINARY_DIR}/test-artifacts/loaded_libraries)
    set_tests_properties(vulkan.loaded_libraries_are_system_only PROPERTIES LABELS "vulkan;display;static" SKIP_RETURN_CODE 77
      TIMEOUT 300 ENVIRONMENT "SOV_REQUIRE_VALIDATION=1")
  endif()
endif()

add_test(NAME static.license_notices_match_lock
  COMMAND "${CMAKE_COMMAND}" -DSOURCE_DIR=${PROJECT_SOURCE_DIR} "-DFETCHED=${SOV_FETCHED_SOURCE_DIRS}"
    -P "${PROJECT_SOURCE_DIR}/cmake/CheckLicenseNotices.cmake")
set_tests_properties(static.license_notices_match_lock PROPERTIES LABELS "headless;static")

# --- owner's packet: verbatim copies and contract conformance --------------------------------
add_test(NAME static.packet_files_verbatim
  COMMAND "${CMAKE_COMMAND}" -DSOURCE_DIR=${PROJECT_SOURCE_DIR} -P "${PROJECT_SOURCE_DIR}/cmake/CheckPacketCopies.cmake")
set_tests_properties(static.packet_files_verbatim PROPERTIES LABELS "headless;static")
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  # The packet's own fixture checker, run unmodified against the repository's copies.
  # It exits 2 when jsonschema is missing: reported as skipped, never as passed.
  add_test(NAME static.packet_contract_checker
    COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/tools/validate_contract_examples.py")
  set_tests_properties(static.packet_contract_checker PROPERTIES LABELS "headless;static" SKIP_RETURN_CODE 2)
  # Every scene and batch in the repository (engine-written ones included) against the owner's schemas.
  add_test(NAME static.repository_files_conform_to_owner_schemas
    COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/scripts/check_contracts.py")
  set_tests_properties(static.repository_files_conform_to_owner_schemas PROPERTIES LABELS "headless;static" SKIP_RETURN_CODE 77)
else()
  message(STATUS "Python 3 not found: packet contract checks are not registered (report them as not run)")
endif()
