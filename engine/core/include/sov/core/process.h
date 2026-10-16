#pragma once

#include <cstdint>

namespace sov {

// Resident memory of this process, for lifecycle and performance reports. Zero when the platform
// query is unavailable (reported as such, never guessed).
struct ProcessMemory {
  std::uint64_t rss_bytes = 0;       // current resident set
  std::uint64_t peak_rss_bytes = 0;  // high-water mark of the resident set
  bool available = false;
};

ProcessMemory QueryProcessMemory();

}  // namespace sov
