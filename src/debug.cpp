#include "debug.h"

#include <iostream>

namespace coconut::debug {

void log(const std::string& msg) {
  std::cerr << "[DEBUG] " << msg << '\n';
}

void info(const std::string& msg) {
  std::cerr << "[INFO]  " << msg << '\n';
}

void warn(const std::string& msg) {
  std::cerr << "[WARN]  " << msg << '\n';
}

void error(const std::string& msg) {
  std::cerr << "[ERROR] " << msg << '\n';
}

} // namespace coconut::debug
