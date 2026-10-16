# Dependency acquisition. Every pin is read from dependencies.lock.json; nothing here names a version.
#
# Method "git": shallow clone of selected_tag, then HEAD is verified against the locked revision
#               (configure fails on mismatch, so a moved tag cannot silently change the build).
# Method "url": archive download verified with URL_HASH SHA256.

include(FetchContent)

set(SOV_LOCK_FILE "${PROJECT_SOURCE_DIR}/dependencies.lock.json")
file(READ "${SOV_LOCK_FILE}" SOV_LOCK_JSON)
file(SHA256 "${SOV_LOCK_FILE}" SOV_DEPENDENCY_LOCK_SHA256)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SOV_LOCK_FILE}")

find_package(Git REQUIRED)

function(_sov_lock_index name out_var)
  string(JSON count LENGTH "${SOV_LOCK_JSON}" dependencies)
  math(EXPR last "${count} - 1")
  foreach(i RANGE ${last})
    string(JSON entry_name GET "${SOV_LOCK_JSON}" dependencies ${i} name)
    if(entry_name STREQUAL name)
      set(${out_var} ${i} PARENT_SCOPE)
      return()
    endif()
  endforeach()
  message(FATAL_ERROR "dependencies.lock.json has no entry named '${name}'")
endfunction()

function(_sov_lock_get out_var index)
  string(JSON value ERROR_VARIABLE err GET "${SOV_LOCK_JSON}" dependencies ${index} ${ARGN})
  if(err)
    set(value "")
  endif()
  set(${out_var} "${value}" PARENT_SCOPE)
endfunction()

# Applies the entry's build_options as cache values before the dependency's own CMake runs.
function(_sov_apply_build_options index)
  string(JSON options ERROR_VARIABLE err GET "${SOV_LOCK_JSON}" dependencies ${index} build_options)
  if(err)
    return()
  endif()
  string(JSON count LENGTH "${options}")
  if(count EQUAL 0)
    return()
  endif()
  math(EXPR last "${count} - 1")
  foreach(i RANGE ${last})
    string(JSON key MEMBER "${options}" ${i})
    string(JSON value GET "${options}" ${key})
    set(${key} "${value}" CACHE INTERNAL "Pinned by dependencies.lock.json")
  endforeach()
endfunction()

# sov_fetch(<lock name> [NO_ADD_SUBDIRECTORY] [PLATFORM <key>])
# Sets <lowercase name>_SOURCE_DIR in the caller's scope.
function(sov_fetch name)
  cmake_parse_arguments(ARG "NO_ADD_SUBDIRECTORY" "PLATFORM" "" ${ARGN})
  _sov_lock_index(${name} index)
  _sov_lock_get(method ${index} method)
  string(TOLOWER "${name}" fc_name)
  string(REPLACE "-" "_" fc_name "${fc_name}")

  _sov_apply_build_options(${index})

  set(subdir_args "")
  if(ARG_NO_ADD_SUBDIRECTORY)
    # FetchContent_MakeAvailable only calls add_subdirectory() when this path holds a CMakeLists.txt.
    set(subdir_args SOURCE_SUBDIR "sov-no-cmake-entry")
  endif()

  if(method STREQUAL "git")
    _sov_lock_get(origin ${index} origin)
    _sov_lock_get(tag ${index} selected_tag)
    _sov_lock_get(revision ${index} revision)
    FetchContent_Declare(${fc_name}
      GIT_REPOSITORY "${origin}.git"
      GIT_TAG "${tag}"
      GIT_SHALLOW TRUE
      GIT_SUBMODULES ""
      ${subdir_args})
  elseif(method STREQUAL "url")
    if(ARG_PLATFORM)
      _sov_lock_get(url ${index} platforms ${ARG_PLATFORM} url)
      _sov_lock_get(sha ${index} platforms ${ARG_PLATFORM} archive_sha256)
    else()
      _sov_lock_get(url ${index} url)
      _sov_lock_get(sha ${index} archive_sha256)
    endif()
    if(url STREQUAL "" OR sha STREQUAL "")
      message(FATAL_ERROR "Lock entry '${name}' has no url/archive_sha256 for platform '${ARG_PLATFORM}'")
    endif()
    FetchContent_Declare(${fc_name}
      URL "${url}"
      URL_HASH SHA256=${sha}
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
      ${subdir_args})
  else()
    message(FATAL_ERROR "Lock entry '${name}' has unsupported method '${method}'")
  endif()

  FetchContent_MakeAvailable(${fc_name})
  FetchContent_GetProperties(${fc_name} SOURCE_DIR source_dir)

  if(method STREQUAL "git")
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" -C "${source_dir}" rev-parse HEAD
      OUTPUT_VARIABLE head OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0 OR NOT head STREQUAL revision)
      message(FATAL_ERROR
        "Dependency '${name}': tag '${tag}' resolved to '${head}', but dependencies.lock.json pins '${revision}'. "
        "Refusing to build with an unverified revision.")
    endif()
    message(STATUS "Dependency ${name}: ${tag} @ ${revision} (verified)")
  else()
    message(STATUS "Dependency ${name}: ${url} (sha256 ${sha})")
  endif()

  set(${fc_name}_SOURCE_DIR "${source_dir}" PARENT_SCOPE)
endfunction()

function(sov_fetch_headless_dependencies)
  sov_fetch(nlohmann_json)
  sov_fetch(EnTT NO_ADD_SUBDIRECTORY)
  add_library(sov_third_party_entt INTERFACE)
  target_include_directories(sov_third_party_entt SYSTEM INTERFACE "${entt_SOURCE_DIR}/src")
  add_library(sov::entt ALIAS sov_third_party_entt)
  set(SOV_FETCHED_SOURCE_DIRS
    "nlohmann_json=${nlohmann_json_SOURCE_DIR}"
    "EnTT=${entt_SOURCE_DIR}"
    PARENT_SCOPE)
endfunction()

function(sov_fetch_graphics_dependencies)
  sov_fetch(Vulkan-Headers)
  sov_fetch(volk NO_ADD_SUBDIRECTORY)
  sov_fetch(VulkanMemoryAllocator NO_ADD_SUBDIRECTORY)
  sov_fetch(SDL3)
  sov_fetch(imgui NO_ADD_SUBDIRECTORY)

  if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(dxc_platform "windows-x64")
    set(dxc_rel "bin/x64/dxc.exe")
  elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(dxc_platform "linux-x86_64")
    set(dxc_rel "bin/dxc")
  else()
    set(dxc_platform "")
  endif()

  set(SOV_DXC_EXECUTABLE "" CACHE FILEPATH "Override path to a DXC executable (default: pinned package from the lock file)")
  if(SOV_DXC_EXECUTABLE STREQUAL "")
    if(dxc_platform STREQUAL "")
      message(FATAL_ERROR "No pinned DXC package for host ${CMAKE_HOST_SYSTEM_NAME}/${CMAKE_HOST_SYSTEM_PROCESSOR}; set SOV_DXC_EXECUTABLE and record it.")
    endif()
    sov_fetch(dxc NO_ADD_SUBDIRECTORY PLATFORM ${dxc_platform})
    set(SOV_DXC_EXECUTABLE "${dxc_SOURCE_DIR}/${dxc_rel}" CACHE FILEPATH "" FORCE)
  endif()
  message(STATUS "Shader compiler: ${SOV_DXC_EXECUTABLE}")

  # volk: compiled once, loads the system Vulkan loader at runtime; no link-time libvulkan dependency.
  add_library(sov_third_party_volk STATIC "${volk_SOURCE_DIR}/volk.c")
  target_include_directories(sov_third_party_volk SYSTEM PUBLIC "${volk_SOURCE_DIR}")
  target_link_libraries(sov_third_party_volk PUBLIC Vulkan::Headers ${CMAKE_DL_LIBS})
  target_compile_definitions(sov_third_party_volk PUBLIC VK_NO_PROTOTYPES)
  if(WIN32)
    target_compile_definitions(sov_third_party_volk PUBLIC VK_USE_PLATFORM_WIN32_KHR)
  endif()
  add_library(sov::volk ALIAS sov_third_party_volk)

  add_library(sov_third_party_vma INTERFACE)
  target_include_directories(sov_third_party_vma SYSTEM INTERFACE "${vulkanmemoryallocator_SOURCE_DIR}/include")
  target_link_libraries(sov_third_party_vma INTERFACE sov::volk)
  add_library(sov::vma ALIAS sov_third_party_vma)

  # Dear ImGui (docking) core + SDL3 platform backend + Vulkan renderer backend. Editor-only.
  set(im "${imgui_SOURCE_DIR}")
  add_library(sov_third_party_imgui STATIC
    "${im}/imgui.cpp" "${im}/imgui_draw.cpp" "${im}/imgui_tables.cpp" "${im}/imgui_widgets.cpp"
    "${im}/backends/imgui_impl_sdl3.cpp" "${im}/backends/imgui_impl_vulkan.cpp")
  target_include_directories(sov_third_party_imgui SYSTEM PUBLIC "${im}" "${im}/backends")
  target_compile_definitions(sov_third_party_imgui PUBLIC IMGUI_IMPL_VULKAN_USE_VOLK IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
  target_link_libraries(sov_third_party_imgui PUBLIC sov::volk SDL3::SDL3-static)
  add_library(sov::imgui ALIAS sov_third_party_imgui)

  set(dirs ${SOV_FETCHED_SOURCE_DIRS}
    "SDL3=${sdl3_SOURCE_DIR}"
    "Vulkan-Headers=${vulkan_headers_SOURCE_DIR}"
    "volk=${volk_SOURCE_DIR}"
    "VulkanMemoryAllocator=${vulkanmemoryallocator_SOURCE_DIR}"
    "imgui=${imgui_SOURCE_DIR}")
  if(DEFINED dxc_SOURCE_DIR)
    list(APPEND dirs "dxc=${dxc_SOURCE_DIR}")
  endif()
  set(SOV_FETCHED_SOURCE_DIRS ${dirs} PARENT_SCOPE)
endfunction()
