#include "sov/core/build_info.h"

#include <format>

#include "sov/build_info.h"

namespace sov {

const BuildInfo& GetBuildInfo() {
  static const BuildInfo info{SOV_VERSION,  SOV_GIT_COMMIT, SOV_GIT_TREE_STATE, SOV_DEPENDENCY_LOCK_SHA256,
                              SOV_BUILD_TYPE, SOV_COMPILER, SOV_TARGET_SYSTEM, SOV_GRAPHICS_BUILD != 0};
  return info;
}

std::string DescribeBuild(const char* executable_name) {
  const BuildInfo& b = GetBuildInfo();
  return std::format(
      "{} {}\n"
      "commit: {} ({})\n"
      "dependency-lock: sha256:{}\n"
      "build: {} | {} | {}\n"
      "contracts: sovereign.scene schema v{}, sovereign.command_batch protocol v{}, journal format v{}\n",
      executable_name, b.version, b.git_commit, b.git_tree_state, b.dependency_lock_sha256, b.build_type,
      b.compiler, b.target_system, kSceneSchemaVersion, kCommandProtocolVersion, kJournalFormatVersion);
}

}  // namespace sov
