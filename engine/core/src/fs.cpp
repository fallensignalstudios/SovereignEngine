#include "sov/core/fs.h"

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <format>
#include <fstream>
#include <random>
#include <system_error>

#include "sov/core/log.h"

#if defined(_WIN32)
#include <windows.h>
#include <process.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace sov::fs {
namespace {

FailPoint g_fail_point = FailPoint::None;
int g_fail_countdown = 0;

bool ShouldFail(FailPoint point) {
  if (g_fail_point != point) return false;
  if (g_fail_countdown > 0) {
    --g_fail_countdown;
    return false;
  }
  g_fail_point = FailPoint::None;  // one-shot
  return true;
}

std::string ErrnoText(int err) { return std::error_code(err, std::generic_category()).message(); }

std::string TempNameFor(const std::filesystem::path& target) {
  static std::atomic<unsigned> counter{0};
  std::random_device rd;
#if defined(_WIN32)
  const auto pid = static_cast<unsigned long>(_getpid());
#else
  const auto pid = static_cast<unsigned long>(getpid());
#endif
  return std::format(".{}.tmp-{}-{:08x}-{}", target.filename().string(), pid, rd(), counter++);
}

#if !defined(_WIN32)
Result<void> WriteAllFd(int fd, std::string_view bytes, const std::filesystem::path& p) {
  std::size_t written = 0;
  while (written < bytes.size()) {
    const ssize_t n = ::write(fd, bytes.data() + written, bytes.size() - written);
    if (n < 0) {
      if (errno == EINTR) continue;
      return MakeError(ErrorCode::IoFailure, std::format("write failed for {}: {}", p.string(), ErrnoText(errno)));
    }
    written += static_cast<std::size_t>(n);
  }
  return Ok();
}

void SyncDirectory(const std::filesystem::path& dir) {
  const int dfd = ::open(dir.empty() ? "." : dir.c_str(), O_RDONLY | O_DIRECTORY);
  if (dfd >= 0) {
    ::fsync(dfd);
    ::close(dfd);
  }
}
#endif

}  // namespace

void SetFailPointForTesting(FailPoint point, int trigger_after_n_calls) {
  g_fail_point = point;
  g_fail_countdown = trigger_after_n_calls;
}

FailPoint ActiveFailPointForTesting() { return g_fail_point; }

Result<std::string> ReadFile(const std::filesystem::path& path, std::size_t max_bytes) {
  std::error_code ec;
  const auto size = std::filesystem::file_size(path, ec);
  if (ec) {
    return MakeError(ErrorCode::IoFailure, std::format("cannot stat {}: {}", path.string(), ec.message()));
  }
  if (size > max_bytes) {
    return MakeError(ErrorCode::LimitExceeded,
                     std::format("{} is {} bytes; limit is {} bytes", path.string(), size, max_bytes));
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) return MakeError(ErrorCode::IoFailure, std::format("cannot open {}", path.string()));
  std::string data(static_cast<std::size_t>(size), '\0');
  in.read(data.data(), static_cast<std::streamsize>(size));
  if (in.gcount() != static_cast<std::streamsize>(size)) {
    return MakeError(ErrorCode::IoFailure, std::format("short read of {}", path.string()));
  }
  return data;
}

Result<void> AtomicWriteFile(const std::filesystem::path& target, std::string_view bytes) {
  const std::filesystem::path dir = target.has_parent_path() ? target.parent_path() : std::filesystem::path(".");
  const std::filesystem::path temp = dir / TempNameFor(target);

#if defined(_WIN32)
  HANDLE h = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    return MakeError(ErrorCode::IoFailure, std::format("cannot create {}: error {}", temp.string(), GetLastError()));
  }
  DWORD written = 0;
  const BOOL ok_write = WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
  const BOOL ok_flush = FlushFileBuffers(h);
  CloseHandle(h);
  if (!ok_write || written != bytes.size() || !ok_flush) {
    DeleteFileW(temp.c_str());
    return MakeError(ErrorCode::IoFailure, std::format("cannot write {}", temp.string()));
  }
  if (ShouldFail(FailPoint::AtomicWriteBeforeRename)) {
    DeleteFileW(temp.c_str());
    return MakeError(ErrorCode::IoFailure, "injected failure before rename (test)");
  }
  if (!MoveFileExW(temp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    const DWORD err = GetLastError();
    DeleteFileW(temp.c_str());
    return MakeError(ErrorCode::IoFailure, std::format("cannot replace {}: error {}", target.string(), err));
  }
  return Ok();
#else
  const int fd = ::open(temp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
  if (fd < 0) {
    return MakeError(ErrorCode::IoFailure, std::format("cannot create {}: {}", temp.string(), ErrnoText(errno)));
  }
  auto written = WriteAllFd(fd, bytes, temp);
  if (written.ok() && ::fsync(fd) != 0) {
    written = MakeError(ErrorCode::IoFailure, std::format("fsync failed for {}: {}", temp.string(), ErrnoText(errno)));
  }
  ::close(fd);
  if (!written.ok()) {
    ::unlink(temp.c_str());
    return written;
  }
  if (ShouldFail(FailPoint::AtomicWriteBeforeRename)) {
    ::unlink(temp.c_str());
    return MakeError(ErrorCode::IoFailure, "injected failure before rename (test)");
  }
  if (::rename(temp.c_str(), target.c_str()) != 0) {
    const int err = errno;
    ::unlink(temp.c_str());
    return MakeError(ErrorCode::IoFailure, std::format("cannot replace {}: {}", target.string(), ErrnoText(err)));
  }
  SyncDirectory(dir);
  return Ok();
#endif
}

Result<void> AppendDurable(const std::filesystem::path& path, std::string_view bytes) {
  if (ShouldFail(FailPoint::AppendBeforeWrite)) {
    return MakeError(ErrorCode::IoFailure, "injected append failure (test)");
  }
#if defined(_WIN32)
  HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    return MakeError(ErrorCode::IoFailure, std::format("cannot open {}: error {}", path.string(), GetLastError()));
  }
  DWORD written = 0;
  const BOOL ok_write = WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
  const BOOL ok_flush = FlushFileBuffers(h);
  CloseHandle(h);
  if (!ok_write || written != bytes.size() || !ok_flush) {
    return MakeError(ErrorCode::IoFailure, std::format("cannot append to {}", path.string()));
  }
  return Ok();
#else
  const bool existed = std::filesystem::exists(path);
  const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
  if (fd < 0) {
    return MakeError(ErrorCode::IoFailure, std::format("cannot open {}: {}", path.string(), ErrnoText(errno)));
  }
  auto written = WriteAllFd(fd, bytes, path);
  if (written.ok() && ::fsync(fd) != 0) {
    written = MakeError(ErrorCode::IoFailure, std::format("fsync failed for {}: {}", path.string(), ErrnoText(errno)));
  }
  ::close(fd);
  if (written.ok() && !existed) SyncDirectory(path.has_parent_path() ? path.parent_path() : ".");
  return written;
#endif
}

std::filesystem::path ExecutableDirectory() {
#if defined(_WIN32)
  wchar_t buf[MAX_PATH];
  const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  return std::filesystem::path(std::wstring(buf, n)).parent_path();
#else
  std::error_code ec;
  const auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
  return ec ? std::filesystem::current_path() : exe.parent_path();
#endif
}

std::string ProcessDescription(std::string_view tool) {
#if defined(_WIN32)
  const auto pid = static_cast<unsigned long>(_getpid());
  char host[256] = "unknown";
  DWORD size = sizeof(host);
  GetComputerNameA(host, &size);
#else
  const auto pid = static_cast<unsigned long>(getpid());
  char host[256] = "unknown";
  gethostname(host, sizeof(host) - 1);
#endif
  return std::format(R"({{"tool":"{}","pid":{},"host":"{}","acquired_at":"{}"}})", tool, pid, host,
                     CurrentUtcTimestamp());
}

Result<std::unique_ptr<FileLock>> FileLock::TryAcquire(const std::filesystem::path& lock_path,
                                                       std::string_view holder_description) {
  std::unique_ptr<FileLock> lock(new FileLock());
  lock->path_ = lock_path;
#if defined(_WIN32)
  HANDLE h = CreateFileW(lock_path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                         nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    return MakeError(ErrorCode::IoFailure, std::format("cannot open lock {}: error {}", lock_path.string(), GetLastError()));
  }
  OVERLAPPED ov{};
  if (!LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &ov)) {
    CloseHandle(h);
    return MakeError(ErrorCode::LockUnavailable,
                     std::format("scene writer lock {} is held by {}", lock_path.string(), ReadHolder(lock_path)));
  }
  {
    // A clean release clears the holder text, so leftover text names an owner that died holding it.
    char buf[512];
    DWORD got = 0;
    SetFilePointer(h, 0, nullptr, FILE_BEGIN);
    if (ReadFile(h, buf, sizeof(buf) - 1, &got, nullptr) && got > 0) {
      std::string previous(buf, got);
      while (!previous.empty() && (previous.back() == '\n' || previous.back() == ' ')) previous.pop_back();
      while (!previous.empty() && previous.front() == ' ') previous.erase(previous.begin());
      lock->reclaimed_from_ = previous;
    }
  }
  if (!lock->reclaimed_from_.empty()) {
    LogWarn("fs", "writer lock {} was left by \"{}\", which ended without releasing it; reclaimed", lock_path.string(),
            lock->reclaimed_from_);
  }
  // The holder description lives after the locked byte range so readers can see it.
  SetFilePointer(h, 0, nullptr, FILE_BEGIN);
  SetEndOfFile(h);
  std::string text = std::string(" ") + std::string(holder_description) + "\n";
  DWORD written = 0;
  WriteFile(h, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
  FlushFileBuffers(h);
  lock->handle_ = h;
#else
  const int fd = ::open(lock_path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
  if (fd < 0) {
    return MakeError(ErrorCode::IoFailure, std::format("cannot open lock {}: {}", lock_path.string(), ErrnoText(errno)));
  }
  if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
    const int err = errno;
    ::close(fd);
    if (err == EWOULDBLOCK) {
      const std::string holder = ReadHolder(lock_path);
      return MakeError(ErrorCode::LockUnavailable,
                       std::format("scene writer lock {} is held by {}", lock_path.string(),
                                   holder.empty() ? std::string("another writer") : holder));
    }
    return MakeError(ErrorCode::IoFailure, std::format("flock failed on {}: {}", lock_path.string(), ErrnoText(err)));
  }
  {
    // A clean release clears the holder text, so leftover text names an owner that died holding it.
    char buf[512];
    const ssize_t n = ::pread(fd, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
      std::string previous(buf, static_cast<std::size_t>(n));
      while (!previous.empty() && (previous.back() == '\n' || previous.back() == ' ')) previous.pop_back();
      lock->reclaimed_from_ = previous;
    }
  }
  if (!lock->reclaimed_from_.empty()) {
    LogWarn("fs",
            "writer lock {} was left by \"{}\", which ended without releasing it; the OS released its lock when it "
            "exited, so this writer now owns it (journal recovery decides any interrupted commit)",
            lock_path.string(), lock->reclaimed_from_);
  }
  if (::ftruncate(fd, 0) == 0) {
    std::string text = std::string(holder_description) + "\n";
    (void)::pwrite(fd, text.data(), text.size(), 0);
    ::fsync(fd);
  }
  lock->fd_ = fd;
#endif
  return lock;
}

std::string FileLock::ReadHolder(const std::filesystem::path& lock_path) {
  std::ifstream in(lock_path, std::ios::binary);
  if (!in) return {};
  std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) text.pop_back();
  while (!text.empty() && text.front() == ' ') text.erase(text.begin());
  if (text.size() > 512) text.resize(512);
  return text;
}

FileLock::~FileLock() {
#if defined(_WIN32)
  if (handle_) {
    HANDLE h = static_cast<HANDLE>(handle_);
    SetFilePointer(h, 0, nullptr, FILE_BEGIN);
    SetEndOfFile(h);
    OVERLAPPED ov{};
    UnlockFileEx(h, 0, 1, 0, &ov);
    CloseHandle(h);
  }
#else
  if (fd_ >= 0) {
    // Clear the holder text while still holding the lock, then release by closing.
    if (::ftruncate(fd_, 0) != 0) {
      LogWarn("fs", "could not clear lock holder text in {}", path_.string());
    }
    ::flock(fd_, LOCK_UN);
    ::close(fd_);
  }
#endif
}

}  // namespace sov::fs
