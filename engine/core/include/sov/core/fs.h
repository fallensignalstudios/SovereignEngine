#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "sov/core/result.h"

namespace sov::fs {

// Reads a whole file. Fails with LimitExceeded when the file is larger than max_bytes
// (checked before allocation) and IoFailure when it cannot be read.
Result<std::string> ReadFile(const std::filesystem::path& path, std::size_t max_bytes);

// Durable single-file replacement:
//   1. write bytes to a uniquely named temporary file in the target's directory
//   2. flush + fsync the temporary file
//   3. rename over the target (atomic on POSIX; MoveFileExW REPLACE_EXISTING|WRITE_THROUGH on Windows)
//   4. fsync the directory (POSIX)
// On any failure before step 3 the previous target file is untouched and the temporary is removed.
// This is a single-file guarantee only; a sequence of AtomicWriteFile calls is not a transaction.
Result<void> AtomicWriteFile(const std::filesystem::path& target, std::string_view bytes);

// Appends bytes and fsyncs before returning (used by the commit journal).
Result<void> AppendDurable(const std::filesystem::path& path, std::string_view bytes);

// Test-only failure injection for the durability paths. Not thread-safe; never set in production code.
enum class FailPoint {
  None,
  AtomicWriteBeforeRename,  // temp file fully written and synced, rename not performed
  AppendBeforeWrite,        // AppendDurable fails without writing
};
void SetFailPointForTesting(FailPoint point, int trigger_after_n_calls = 0);
FailPoint ActiveFailPointForTesting();

// Exclusive advisory writer lock on <path> (POSIX flock / Windows LockFileEx).
// The lock is released by the destructor or when the process exits (the OS drops it),
// so a crashed holder cannot leave a stale lock. The lock file itself is left in place;
// it contains a JSON description of the current holder for diagnostics.
class FileLock {
 public:
  // Fails with LockUnavailable (message includes the holder description when readable).
  static Result<std::unique_ptr<FileLock>> TryAcquire(const std::filesystem::path& lock_path,
                                                      std::string_view holder_description);
  // Best-effort read of the current holder description; empty when unknown.
  static std::string ReadHolder(const std::filesystem::path& lock_path);

  ~FileLock();
  FileLock(const FileLock&) = delete;
  FileLock& operator=(const FileLock&) = delete;

  const std::filesystem::path& path() const { return path_; }
  // Holder record left in the lock file by a previous owner that ended without releasing the lock
  // (crash, kill). Empty after a clean release. The OS lock itself died with that process, so
  // acquiring was safe; this makes the reclaim explicit to callers and logs.
  const std::string& reclaimed_from() const { return reclaimed_from_; }

 private:
  FileLock() = default;
  std::filesystem::path path_;
  std::string reclaimed_from_;
#if defined(_WIN32)
  void* handle_ = nullptr;
#else
  int fd_ = -1;
#endif
};

// Directory containing the running executable (shaders and other runtime files live beside it).
std::filesystem::path ExecutableDirectory();

// Short description of this process for lock files and journal records.
std::string ProcessDescription(std::string_view tool);

}  // namespace sov::fs
