#include "test.h"

#include <exception>

int main() {
  try {
    return coconut::test::run_all();
  } catch (const std::exception &e) {
    std::cerr << "fatal test runner error: " << e.what() << '\n';
    return 1;
  }
}
