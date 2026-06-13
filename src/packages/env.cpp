#include "env.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace coconut::env {

std::string get(const std::string& name) {
  if (name.empty()) return {};

#if defined(_WIN32)
  // _dupenv_s handles NULL correctly on Windows.
  char* buf = nullptr;
  size_t len = 0;
  if (_dupenv_s(&buf, &len, name.c_str()) != 0 || buf == nullptr) {
    return {};
  }
  std::string result(buf);
  free(buf);
  return result;
#else
  const char* val = std::getenv(name.c_str());
  return val ? std::string(val) : std::string();
#endif
}

std::string cwd() {
  auto p = std::filesystem::current_path().u8string();
  return std::string(reinterpret_cast<const char*>(p.data()), p.size());
}

std::string homedir() {
#if defined(_WIN32)
  return get("USERPROFILE");
#else
  return get("HOME");
#endif
}

char pathSeparator() {
#if defined(_WIN32)
  return ';';
#else
  return ':';
#endif
}

} // namespace coconut::env