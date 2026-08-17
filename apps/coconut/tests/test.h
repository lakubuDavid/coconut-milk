#ifndef COCONUT_TEST_H
#define COCONUT_TEST_H

#include "error.h"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace coconut::test {

/// Skip reason for a test case.
struct SkipInfo {
  const char* reason;
  bool active;
};

struct TestCase {
  std::string suite;
  std::string name;
  void (*fn)();
  SkipInfo skip;
};

std::vector<TestCase> &registry();

struct Registrar {
  Registrar(const char *suite, const char *name, void (*fn)());
  Registrar(const char *suite, const char *name, void (*fn)(), const char* skip_reason);
};

void require(bool condition, const char *expr, const char *file, int line);
void require_equal(const std::string &lhs, const std::string &rhs, const char *lhs_expr, const char *rhs_expr, const char *file, int line);
void require_equal(int lhs, int rhs, const char *lhs_expr, const char *rhs_expr, const char *file, int line);
void require_equal(coconut::ErrorCode lhs, coconut::ErrorCode rhs, const char *lhs_expr, const char *rhs_expr, const char *file, int line);

template <typename T>
void require_equal(T lhs, T rhs, const char *lhs_expr, const char *rhs_expr, const char *file, int line) {
  if (lhs != rhs) {
    throw std::runtime_error(std::string(file) + ":" + std::to_string(line) +
                             ": equality failed: " + lhs_expr + " == " + rhs_expr +
                             " (lhs=" + std::to_string(static_cast<int>(lhs)) +
                             ", rhs=" + std::to_string(static_cast<int>(rhs)) + ")");
  }
}

int run_all();

} // namespace coconut::test

#define COCONUT_TEST_CONCAT_INNER(a, b) a##b
#define COCONUT_TEST_CONCAT(a, b) COCONUT_TEST_CONCAT_INNER(a, b)

#define COCONUT_TEST(suite, name)                                                                  \
  static void COCONUT_TEST_CONCAT(test_fn_, __LINE__)();                                           \
  static ::coconut::test::Registrar COCONUT_TEST_CONCAT(test_reg_, __LINE__)(#suite, #name,        \
                                                                              COCONUT_TEST_CONCAT(test_fn_, __LINE__)); \
  static void COCONUT_TEST_CONCAT(test_fn_, __LINE__)()

#define COCONUT_TEST_SKIP(suite, name, reason)                                                     \
  static void COCONUT_TEST_CONCAT(test_fn_, __LINE__)();                                           \
  static ::coconut::test::Registrar COCONUT_TEST_CONCAT(test_reg_, __LINE__)(#suite, #name,        \
                                                                              COCONUT_TEST_CONCAT(test_fn_, __LINE__), reason); \
  static void COCONUT_TEST_CONCAT(test_fn_, __LINE__)()

/// Platform skip macros — mark tests that only apply to a specific OS.
/// The test compiles on all platforms but is skipped at runtime.
#if defined(__APPLE__)
  #define COCONUT_TEST_MACOS(suite, name)    COCONUT_TEST(suite, name)
  #define COCONUT_TEST_NOT_MACOS(suite, name, reason) COCONUT_TEST_SKIP(suite, name, reason)
  #define COCONUT_TEST_WIN(suite, name)      COCONUT_TEST_SKIP(suite, name, "windows only")
  #define COCONUT_TEST_LINUX(suite, name)    COCONUT_TEST_SKIP(suite, name, "linux only")
#elif defined(_WIN32)
  #define COCONUT_TEST_MACOS(suite, name)    COCONUT_TEST_SKIP(suite, name, "macos only")
  #define COCONUT_TEST_NOT_MACOS(suite, name, reason) COCONUT_TEST(suite, name)
  #define COCONUT_TEST_WIN(suite, name)      COCONUT_TEST(suite, name)
  #define COCONUT_TEST_LINUX(suite, name)    COCONUT_TEST_SKIP(suite, name, "linux only")
#elif defined(__linux__)
  #define COCONUT_TEST_MACOS(suite, name)    COCONUT_TEST_SKIP(suite, name, "macos only")
  #define COCONUT_TEST_NOT_MACOS(suite, name, reason) COCONUT_TEST(suite, name)
  #define COCONUT_TEST_WIN(suite, name)      COCONUT_TEST_SKIP(suite, name, "windows only")
  #define COCONUT_TEST_LINUX(suite, name)    COCONUT_TEST(suite, name)
#else
  #define COCONUT_TEST_MACOS(suite, name)    COCONUT_TEST_SKIP(suite, name, "macos only")
  #define COCONUT_TEST_NOT_MACOS(suite, name, reason) COCONUT_TEST_SKIP(suite, name, reason)
  #define COCONUT_TEST_WIN(suite, name)      COCONUT_TEST_SKIP(suite, name, "windows only")
  #define COCONUT_TEST_LINUX(suite, name)    COCONUT_TEST_SKIP(suite, name, "linux only")
#endif

#define COCONUT_REQUIRE(expr) ::coconut::test::require(static_cast<bool>(expr), #expr, __FILE__, __LINE__)
#define COCONUT_REQUIRE_EQ(lhs, rhs)                                                                \
  ::coconut::test::require_equal((lhs), (rhs), #lhs, #rhs, __FILE__, __LINE__)

#endif // COCONUT_TEST_H
