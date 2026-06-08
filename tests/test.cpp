#include "test.h"
#include "error.h"

namespace coconut::test {

std::vector<TestCase> &registry() {
  static std::vector<TestCase> tests;
  return tests;
}

Registrar::Registrar(const char *suite, const char *name, void (*fn)()) {
  registry().push_back(TestCase{suite, name, fn});
}

void require(bool condition, const char *expr, const char *file, int line) {
  if (!condition) {
    throw std::runtime_error(std::string(file) + ":" + std::to_string(line) +
                             ": requirement failed: " + expr);
  }
}

void require_equal(const std::string &lhs, const std::string &rhs, const char *lhs_expr, const char *rhs_expr,
                   const char *file, int line) {
  if (lhs != rhs) {
    throw std::runtime_error(std::string(file) + ":" + std::to_string(line) +
                             ": equality failed: " + lhs_expr + " == " + rhs_expr +
                             " (lhs=\"" + lhs + "\", rhs=\"" + rhs + "\")");
  }
}

void require_equal(int lhs, int rhs, const char *lhs_expr, const char *rhs_expr, const char *file, int line) {
  if (lhs != rhs) {
    throw std::runtime_error(std::string(file) + ":" + std::to_string(line) +
                             ": equality failed: " + lhs_expr + " == " + rhs_expr +
                             " (lhs=" + std::to_string(lhs) + ", rhs=" + std::to_string(rhs) + ")");
  }
}

void require_equal(coconut::ErrorCode lhs, coconut::ErrorCode rhs, const char *lhs_expr,
                   const char *rhs_expr, const char *file, int line) {
  if (lhs != rhs) {
    throw std::runtime_error(
        std::string(file) + ":" + std::to_string(line) +
        ": equality failed: " + lhs_expr + " == " + rhs_expr +
        " (lhs=" + std::to_string(static_cast<int>(lhs)) +
        ", rhs=" + std::to_string(static_cast<int>(rhs)) + ")");
  }
}

int run_all() {
  auto &tests = registry();
  int failed = 0;

  for (const auto &test : tests) {
    try {
      test.fn();
      std::cout << "[PASS] " << test.suite << "." << test.name << '\n';
    } catch (const std::exception &e) {
      ++failed;
      std::cerr << "[FAIL] " << test.suite << "." << test.name << " -> " << e.what() << '\n';
    }
  }

  std::cout << "\n" << (tests.size() - failed) << " passed, " << failed << " failed, " << tests.size()
            << " total\n";
  return failed == 0 ? 0 : 1;
}

} // namespace coconut::test
