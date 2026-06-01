#ifndef COCONUT_TEST_H
#define COCONUT_TEST_H

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace coconut::test {

struct TestCase {
  std::string suite;
  std::string name;
  void (*fn)();
};

std::vector<TestCase> &registry();

struct Registrar {
  Registrar(const char *suite, const char *name, void (*fn)());
};

void require(bool condition, const char *expr, const char *file, int line);
void require_equal(const std::string &lhs, const std::string &rhs, const char *lhs_expr, const char *rhs_expr, const char *file, int line);
void require_equal(int lhs, int rhs, const char *lhs_expr, const char *rhs_expr, const char *file, int line);

int run_all();

} // namespace coconut::test

#define COCONUT_TEST_CONCAT_INNER(a, b) a##b
#define COCONUT_TEST_CONCAT(a, b) COCONUT_TEST_CONCAT_INNER(a, b)

#define COCONUT_TEST(suite, name)                                                                  \
  static void COCONUT_TEST_CONCAT(test_fn_, __LINE__)();                                           \
  static ::coconut::test::Registrar COCONUT_TEST_CONCAT(test_reg_, __LINE__)(#suite, #name,        \
                                                                              COCONUT_TEST_CONCAT(test_fn_, __LINE__)); \
  static void COCONUT_TEST_CONCAT(test_fn_, __LINE__)()

#define COCONUT_REQUIRE(expr) ::coconut::test::require((expr), #expr, __FILE__, __LINE__)
#define COCONUT_REQUIRE_EQ(lhs, rhs)                                                                \
  ::coconut::test::require_equal((lhs), (rhs), #lhs, #rhs, __FILE__, __LINE__)

#endif // COCONUT_TEST_H
