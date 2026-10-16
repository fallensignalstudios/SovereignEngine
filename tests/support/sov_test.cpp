#include "sov_test.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <map>

#ifndef SOV_TEST_SOURCE_DIR
#error "SOV_TEST_SOURCE_DIR must be defined by the build"
#endif
#ifndef SOV_TEST_TEMP_ROOT
#error "SOV_TEST_TEMP_ROOT must be defined by the build"
#endif

namespace sov::test {
namespace {

struct Entry {
  std::string suite;
  std::string name;
  TestFn fn;
};

std::vector<Entry>& Registry() {
  static std::vector<Entry> registry;
  return registry;
}

}  // namespace

Registrar::Registrar(const char* suite, const char* name, TestFn fn) { Registry().push_back({suite, name, fn}); }

Context::Context(std::string full_name) : name_(std::move(full_name)) {}

void Context::Fail(const char* file, int line, std::string message) {
  failures_.push_back({file, line, std::move(message)});
}

const std::filesystem::path& Context::TempDir() {
  if (temp_dir_.empty()) {
    temp_dir_ = std::filesystem::path(SOV_TEST_TEMP_ROOT) / name_;
    std::error_code ec;
    std::filesystem::remove_all(temp_dir_, ec);
    std::filesystem::create_directories(temp_dir_);
  }
  return temp_dir_;
}

std::filesystem::path Context::SourceDir() { return std::filesystem::path(SOV_TEST_SOURCE_DIR); }

int RunAll(int argc, char** argv) {
  std::string suite_filter;
  std::string name_filter;
  bool list = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--suite") == 0 && i + 1 < argc) {
      suite_filter = argv[++i];
    } else if (std::strcmp(argv[i], "--filter") == 0 && i + 1 < argc) {
      name_filter = argv[++i];
    } else if (std::strcmp(argv[i], "--list") == 0) {
      list = true;
    } else {
      std::fprintf(stderr, "usage: %s [--suite S] [--filter substring] [--list]\n", argv[0]);
      return 2;
    }
  }

  auto entries = Registry();
  std::stable_sort(entries.begin(), entries.end(),
                   [](const Entry& a, const Entry& b) { return a.suite < b.suite; });

  int run = 0, failed = 0;
  for (const auto& e : entries) {
    if (!suite_filter.empty() && e.suite != suite_filter) continue;
    const std::string full = e.suite + "." + e.name;
    if (!name_filter.empty() && full.find(name_filter) == std::string::npos) continue;
    if (list) {
      std::printf("%s\n", full.c_str());
      continue;
    }
    Context ctx(full);
    const auto start = std::chrono::steady_clock::now();
    try {
      e.fn(ctx);
    } catch (const AbortTest&) {
      // failure already recorded
    } catch (const std::exception& ex) {
      ctx.Fail("<exception>", 0, std::string("unhandled exception: ") + ex.what());
    } catch (...) {
      ctx.Fail("<exception>", 0, "unhandled non-standard exception");
    }
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    ++run;
    for (const auto& note : ctx.notes()) std::printf("    note: %s\n", note.c_str());
    if (ctx.failed()) {
      ++failed;
      std::printf("[FAIL] %s (%.1f ms)\n", full.c_str(), ms);
      for (const auto& f : ctx.failures()) std::printf("       %s:%d: %s\n", f.file.c_str(), f.line, f.message.c_str());
    } else {
      std::printf("[PASS] %s (%.1f ms)\n", full.c_str(), ms);
    }
    std::fflush(stdout);
  }
  if (list) return 0;
  std::printf("%d test(s) run, %d failed\n", run, failed);
  if (run == 0) {
    std::printf("no tests matched the filter\n");
    return 1;
  }
  return failed == 0 ? 0 : 1;
}

}  // namespace sov::test
