#include "sov/core/log.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <utility>
#include <vector>

namespace sov {
namespace {

struct LogState {
  std::mutex mutex;
  std::vector<std::pair<int, LogSink>> sinks;
  int next_id = 1;
  bool stderr_enabled = true;
};

LogState& State() {
  static LogState state;
  return state;
}

std::atomic<int> g_min_level{static_cast<int>(LogLevel::Info)};

}  // namespace

std::string_view ToString(LogLevel level) {
  switch (level) {
    case LogLevel::Debug: return "debug";
    case LogLevel::Info: return "info";
    case LogLevel::Warn: return "warn";
    case LogLevel::Error: return "error";
  }
  return "info";
}

void SetMinimumLogLevel(LogLevel level) { g_min_level = static_cast<int>(level); }
LogLevel MinimumLogLevel() { return static_cast<LogLevel>(g_min_level.load()); }

int AddLogSink(LogSink sink) {
  auto& s = State();
  std::lock_guard lock(s.mutex);
  const int id = s.next_id++;
  s.sinks.emplace_back(id, std::move(sink));
  return id;
}

void RemoveLogSink(int id) {
  auto& s = State();
  std::lock_guard lock(s.mutex);
  std::erase_if(s.sinks, [id](const auto& entry) { return entry.first == id; });
}

void SetStderrLogging(bool enabled) {
  auto& s = State();
  std::lock_guard lock(s.mutex);
  s.stderr_enabled = enabled;
}

std::string CurrentUtcTimestamp() {
  using namespace std::chrono;
  const auto now = system_clock::now();
  const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
  const std::time_t t = system_clock::to_time_t(now);
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", tm.tm_year + 1900, tm.tm_mon + 1,
                tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<int>(ms.count()));
  return buf;
}

void LogMessage(LogLevel level, std::string_view module, std::string_view message) {
  if (static_cast<int>(level) < g_min_level.load()) return;
  const std::string ts = CurrentUtcTimestamp();
  auto& s = State();
  std::lock_guard lock(s.mutex);
  if (s.stderr_enabled) {
    std::fprintf(stderr, "%s [%-5.*s] [%.*s] %.*s\n", ts.c_str(), static_cast<int>(ToString(level).size()),
                 ToString(level).data(), static_cast<int>(module.size()), module.data(),
                 static_cast<int>(message.size()), message.data());
  }
  const LogRecord record{level, module, message, ts};
  for (auto& [id, sink] : s.sinks) sink(record);
}

}  // namespace sov
