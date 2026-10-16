#pragma once

#include <string>

namespace sov {

struct BuildInfo {
  const char* version;
  const char* git_commit;       // commit of the source tree at build time, or "uncommitted"
  const char* git_tree_state;   // "clean", "modified" (tracked files changed) or "unknown"
  const char* dependency_lock_sha256;
  const char* build_type;
  const char* compiler;
  const char* target_system;
  bool graphics_build;          // true when this binary's build enabled SDL3/Vulkan targets
};

const BuildInfo& GetBuildInfo();

// Multi-line human-readable description used by every executable's --version.
std::string DescribeBuild(const char* executable_name);

// Versions of the authoring contracts this build reads and writes.
inline constexpr int kSceneSchemaVersion = 1;
inline constexpr int kCommandProtocolVersion = 1;
inline constexpr int kJournalFormatVersion = 1;

}  // namespace sov
