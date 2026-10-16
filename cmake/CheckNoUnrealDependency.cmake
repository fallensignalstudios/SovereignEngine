# cmake -DSOURCE_DIR=<repo> -P CheckNoUnrealDependency.cmake
# Fails if build scripts or code reference Unreal Engine headers, modules, SDK paths or asset
# formats. Documentation is excluded (it discusses Unreal as context, which is fine).
set(patterns
  "CoreMinimal\\.h" "UObject/" "Engine/Source" "UnrealBuildTool" "UnrealEditor" "\\.uasset" "\\.umap"
  "\\.uproject" "UE_LOG" "UCLASS\\(" "UPROPERTY\\(" "EpicGames" "ToolsetRegistry")
file(GLOB_RECURSE files
  "${SOURCE_DIR}/engine/*" "${SOURCE_DIR}/editor/*" "${SOURCE_DIR}/tools/*" "${SOURCE_DIR}/games/*"
  "${SOURCE_DIR}/tests/*.cpp" "${SOURCE_DIR}/tests/*.h" "${SOURCE_DIR}/cmake/*" "${SOURCE_DIR}/CMakeLists.txt"
  "${SOURCE_DIR}/CMakePresets.json" "${SOURCE_DIR}/dependencies.lock.json" "${SOURCE_DIR}/scripts/*")
set(hits "")
foreach(f IN LISTS files)
  if(f MATCHES "CheckNoUnrealDependency\\.cmake$")
    continue()
  endif()
  file(READ "${f}" text)
  foreach(p IN LISTS patterns)
    if(text MATCHES "${p}")
      list(APPEND hits "${f}: /${p}/")
    endif()
  endforeach()
endforeach()
list(LENGTH files count)
if(hits)
  list(JOIN hits "\n  " t)
  message(FATAL_ERROR "Unreal references found:\n  ${t}")
endif()
message(STATUS "scanned ${count} build/code files: no Unreal headers, modules, SDK paths or asset formats referenced")
