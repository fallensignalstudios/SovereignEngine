#pragma once

#include <format>
#include <functional>
#include <string>
#include <string_view>

namespace sov {

// Log line format (docs/architecture/logging_and_errors.md):
//   2026-10-16T19:40:00.123Z [warn ] [vulkan] message
enum class LogLevel { Debug = 0, Info = 1, Warn = 2, Error = 3 };

std::string_view ToString(LogLevel level);

struct LogRecord {
  LogLevel level;
  std::string_view module;
  std::string_view message;
  std::string_view timestamp;  // ISO-8601 UTC with milliseconds
};

using LogSink = std::function<void(const LogRecord&)>;

void SetMinimumLogLevel(LogLevel level);
LogLevel MinimumLogLevel();

// Default sink writes to stderr. Additional sinks (editor diagnostics panel, log file, tests)
// are appended; the returned id removes them. Sinks are invoked on the logging thread.
int AddLogSink(LogSink sink);
void RemoveLogSink(int id);
void SetStderrLogging(bool enabled);

void LogMessage(LogLevel level, std::string_view module, std::string_view message);

template <class... Args>
void LogInfo(std::string_view module, std::format_string<Args...> fmt, Args&&... args) {
  if (MinimumLogLevel() <= LogLevel::Info) LogMessage(LogLevel::Info, module, std::format(fmt, std::forward<Args>(args)...));
}
template <class... Args>
void LogWarn(std::string_view module, std::format_string<Args...> fmt, Args&&... args) {
  if (MinimumLogLevel() <= LogLevel::Warn) LogMessage(LogLevel::Warn, module, std::format(fmt, std::forward<Args>(args)...));
}
template <class... Args>
void LogError(std::string_view module, std::format_string<Args...> fmt, Args&&... args) {
  LogMessage(LogLevel::Error, module, std::format(fmt, std::forward<Args>(args)...));
}
template <class... Args>
void LogDebug(std::string_view module, std::format_string<Args...> fmt, Args&&... args) {
  if (MinimumLogLevel() <= LogLevel::Debug) LogMessage(LogLevel::Debug, module, std::format(fmt, std::forward<Args>(args)...));
}

std::string CurrentUtcTimestamp();

}  // namespace sov
