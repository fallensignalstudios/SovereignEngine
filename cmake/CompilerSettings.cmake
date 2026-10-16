# Engine-owned targets call sov_configure_target(); third-party targets keep their own flags.

set(SOV_SANITIZE "" CACHE STRING "Semicolon list of sanitizers for GCC/Clang (e.g. address;undefined)")

if(SOV_SANITIZE AND NOT MSVC)
  list(JOIN SOV_SANITIZE "," _sov_san)
  # Applied globally so dependencies and engine code agree on instrumentation (required for ASan containers).
  add_compile_options(-fsanitize=${_sov_san} -fno-omit-frame-pointer -fno-sanitize-recover=all)
  add_link_options(-fsanitize=${_sov_san})
  message(STATUS "Sanitizers enabled: ${_sov_san}")
endif()

function(sov_configure_target target)
  target_compile_features(${target} PUBLIC cxx_std_20)
  set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
    if(SOV_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
    target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
  else()
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Woverloaded-virtual
      -Wcast-align -Wmissing-declarations -Wformat=2 -Wimplicit-fallthrough)
    if(SOV_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()
