#pragma once

// Small native assertion harness (TDD §4.1: adopt a larger framework only when useful).
//
//   SOV_TEST(suite, name) { SOV_CHECK(x); SOV_CHECK_EQ(a, b); SOV_REQUIRE(ptr); }
//
// SOV_CHECK* records a failure and continues; SOV_REQUIRE* records and aborts the test.
// Run: sovereign-tests [--suite S] [--filter substring] [--list]

#include <filesystem>
#include <functional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace sov::test {

struct Failure {
  std::string file;
  int line;
  std::string message;
};

class Context {
 public:
  explicit Context(std::string full_name);
  void Fail(const char* file, int line, std::string message);
  bool failed() const { return !failures_.empty(); }
  const std::vector<Failure>& failures() const { return failures_; }

  // Fresh empty directory unique to this test, under <build>/test-tmp/<suite>.<name>/
  const std::filesystem::path& TempDir();
  // Repository root (configured at build time).
  static std::filesystem::path SourceDir();

  void Note(std::string text) { notes_.push_back(std::move(text)); }
  const std::vector<std::string>& notes() const { return notes_; }

 private:
  std::string name_;
  std::vector<Failure> failures_;
  std::vector<std::string> notes_;
  std::filesystem::path temp_dir_;
};

struct AbortTest {};

using TestFn = void (*)(Context&);
struct Registrar {
  Registrar(const char* suite, const char* name, TestFn fn);
};

template <class T>
std::string Show(const T& v) {
  if constexpr (requires(std::ostream& os) { os << v; }) {
    std::ostringstream os;
    os << v;
    return os.str();
  } else {
    return "<value>";
  }
}

int RunAll(int argc, char** argv);

}  // namespace sov::test

#define SOV_TEST_CONCAT2(a, b) a##b
#define SOV_TEST_CONCAT(a, b) SOV_TEST_CONCAT2(a, b)

#define SOV_TEST(suite, name)                                                                         \
  static void SOV_TEST_CONCAT(sov_test_fn_, SOV_TEST_CONCAT(suite, SOV_TEST_CONCAT(_, name)))(        \
      ::sov::test::Context&);                                                                         \
  static ::sov::test::Registrar SOV_TEST_CONCAT(sov_test_reg_, SOV_TEST_CONCAT(suite, SOV_TEST_CONCAT(_, name)))( \
      #suite, #name, &SOV_TEST_CONCAT(sov_test_fn_, SOV_TEST_CONCAT(suite, SOV_TEST_CONCAT(_, name)))); \
  static void SOV_TEST_CONCAT(sov_test_fn_, SOV_TEST_CONCAT(suite, SOV_TEST_CONCAT(_, name)))(        \
      [[maybe_unused]] ::sov::test::Context& ctx)

#define SOV_CHECK(expr)                                              \
  do {                                                               \
    if (!(expr)) ctx.Fail(__FILE__, __LINE__, "CHECK(" #expr ") failed"); \
  } while (0)

#define SOV_CHECK_MSG(expr, msg)                                                       \
  do {                                                                                 \
    if (!(expr)) ctx.Fail(__FILE__, __LINE__, std::string("CHECK(" #expr ") failed: ") + (msg)); \
  } while (0)

#define SOV_CHECK_EQ(a, b)                                                                                \
  do {                                                                                                    \
    const auto sov_a_ = (a);                                                                              \
    const auto sov_b_ = (b);                                                                              \
    if (!(sov_a_ == sov_b_))                                                                              \
      ctx.Fail(__FILE__, __LINE__,                                                                          \
             "CHECK_EQ(" #a ", " #b ") failed: " + ::sov::test::Show(sov_a_) + " != " + ::sov::test::Show(sov_b_)); \
  } while (0)

#define SOV_REQUIRE(expr)                                                  \
  do {                                                                     \
    if (!(expr)) {                                                         \
      ctx.Fail(__FILE__, __LINE__, "REQUIRE(" #expr ") failed");             \
      throw ::sov::test::AbortTest{};                                      \
    }                                                                      \
  } while (0)

#define SOV_REQUIRE_MSG(expr, msg)                                                               \
  do {                                                                                           \
    if (!(expr)) {                                                                               \
      ctx.Fail(__FILE__, __LINE__, std::string("REQUIRE(" #expr ") failed: ") + (msg));            \
      throw ::sov::test::AbortTest{};                                                            \
    }                                                                                            \
  } while (0)

// Requires a sov::Result to be ok, printing its error otherwise.
#define SOV_REQUIRE_OK(result_expr)                                                              \
  do {                                                                                           \
    const auto& sov_r_ = (result_expr);                                                          \
    if (!sov_r_.ok()) {                                                                          \
      ctx.Fail(__FILE__, __LINE__, "REQUIRE_OK(" #result_expr ") failed: " + sov_r_.error().Describe()); \
      throw ::sov::test::AbortTest{};                                                            \
    }                                                                                            \
  } while (0)
