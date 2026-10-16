cmake_minimum_required(VERSION 3.25)

project(SovereignEngine
  VERSION 0.1.0
  DESCRIPTION "Sovereign Engine research build (First Ignition)"
  LANGUAGES C CXX)

option(SOV_ENABLE_GRAPHICS "Build SDL3/Vulkan targets: renderer, editor, runtime, rendering tests" OFF)
option(SOV_WARNINGS_AS_ERRORS "Treat warnings in engine-owned code as errors" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are not supported. Use: cmake --preset dev-headless")
endif()

include(cmake/CompilerSettings.cmake)
include(cmake/Dependencies.cmake)

sov_fetch_headless_dependencies()
if(SOV_ENABLE_GRAPHICS)
  sov_fetch_graphics_dependencies()
  include(cmake/Shaders.cmake)
endif()

# Build information header, regenerated at build time.
set(SOV_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(SOV_BUILD_INFO_HEADER "${SOV_GENERATED_DIR}/sov/build_info.h")
if(SOV_ENABLE_GRAPHICS)
  set(_sov_graphics 1)
else()
  set(_sov_graphics 0)
endif()
add_custom_target(sov_build_info
  COMMAND "${CMAKE_COMMAND}"
    -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
    -DOUTPUT=${SOV_BUILD_INFO_HEADER}
    -DVERSION=${PROJECT_VERSION}
    -DLOCK_SHA256=${SOV_DEPENDENCY_LOCK_SHA256}
    -DBUILD_TYPE=${CMAKE_BUILD_TYPE}
    "-DCOMPILER=${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
    "-DSYSTEM=${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}"
    -DGRAPHICS=${_sov_graphics}
    -P "${PROJECT_SOURCE_DIR}/cmake/WriteBuildInfo.cmake"
  BYPRODUCTS "${SOV_BUILD_INFO_HEADER}"
  COMMENT "Updating build information"
  VERBATIM)

enable_testing()

add_subdirectory(engine)
add_subdirectory(games/sandbox)
add_subdirectory(editor)
add_subdirectory(tools/cli)
add_subdirectory(tests)
