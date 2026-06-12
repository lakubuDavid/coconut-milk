#include "routes.h"
#include "debug.h"

#include <format>

namespace coconut::routes {

static std::set<std::string> g_view_names;

void setViewNames(const std::set<std::string>& names) {
  g_view_names = names;
  std::string joined;
  for (const auto& n : names) {
    if (!joined.empty()) joined += ", ";
    joined += n;
  }
  debug::info(std::format("routes: {} view(s) registered [{}]", names.size(), joined));
}

std::string resolve(std::string_view path) {
  if (path.empty()) {
    debug::info("routes: resolve('') -> empty (no path)");
    return {};
  }

  auto it = g_view_names.find(std::string(path));
  if (it != g_view_names.end()) {
    debug::info(std::format("routes: resolve('{}') -> view '{}'", path, *it));
    return *it;
  }

  debug::info(std::format("routes: resolve('{}') -> empty (not a view name, will try file)", path));
  return {};
}

const std::set<std::string>& getViewNames() {
  return g_view_names;
}

} // namespace coconut::routes
