#include "sov/core/process.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <cstdio>
#include <cstring>
#endif

namespace sov {

ProcessMemory QueryProcessMemory() {
  ProcessMemory m;
#if defined(_WIN32)
  PROCESS_MEMORY_COUNTERS pmc{};
  pmc.cb = sizeof(pmc);
  if (K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
    m.rss_bytes = pmc.WorkingSetSize;
    m.peak_rss_bytes = pmc.PeakWorkingSetSize;
    m.available = true;
  }
#elif defined(__linux__)
  // /proc/self/status: "VmRSS:   123456 kB", "VmHWM:   234567 kB".
  if (std::FILE* f = std::fopen("/proc/self/status", "r")) {
    char line[256];
    unsigned long long kb = 0;
    while (std::fgets(line, sizeof(line), f)) {
      if (std::sscanf(line, "VmRSS: %llu kB", &kb) == 1) m.rss_bytes = kb * 1024ull;
      if (std::sscanf(line, "VmHWM: %llu kB", &kb) == 1) m.peak_rss_bytes = kb * 1024ull;
    }
    std::fclose(f);
    m.available = m.rss_bytes > 0;
  }
#endif
  return m;
}

}  // namespace sov
