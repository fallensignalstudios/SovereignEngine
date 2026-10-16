# Offline HLSL -> SPIR-V compilation with the pinned DXC package.
# Flags are part of the shader contract (docs/architecture/conventions.md):
#   -spirv -fspv-target-env=vulkan1.3   SPIR-V for the Vulkan 1.3 profile
#   -Zpc                                 column-major matrix packing (matches sov::Mat4 storage)
#   -HV 2021                             HLSL 2021 language rules
#   -WX -O3                              warnings are errors; optimized output
# Shaders are written to <build>/bin/shaders so executables find them next to themselves.

set(SOV_SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/shaders")
set(SOV_DXC_FLAGS -spirv -fspv-target-env=vulkan1.3 -Zpc -HV 2021 -WX -O3)

# sov_compile_shaders(<out_list_var> SOURCES <file.hlsl|stage|entry> ...)
#   stage is vs, ps or cs; output: <name>.<entry>.spv
function(sov_compile_shaders out_var)
  cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDS" ${ARGN})
  set(outputs "")
  foreach(spec IN LISTS ARG_SOURCES)
    string(REPLACE "|" ";" parts "${spec}")
    list(GET parts 0 source)
    list(GET parts 1 stage)
    list(GET parts 2 entry)
    get_filename_component(abs_source "${source}" ABSOLUTE)
    get_filename_component(name "${source}" NAME_WE)
    set(output "${SOV_SHADER_OUTPUT_DIR}/${name}.${entry}.spv")
    add_custom_command(
      OUTPUT "${output}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${SOV_SHADER_OUTPUT_DIR}"
      COMMAND "${SOV_DXC_EXECUTABLE}" ${SOV_DXC_FLAGS} -T ${stage}_6_0 -E ${entry}
              -Fo "${output}" "${abs_source}"
      DEPENDS "${abs_source}" ${ARG_DEPENDS}
      COMMENT "DXC ${name}.hlsl (${stage}, ${entry}) -> SPIR-V"
      VERBATIM)
    list(APPEND outputs "${output}")
  endforeach()
  set(${out_var} ${outputs} PARENT_SCOPE)
endfunction()
