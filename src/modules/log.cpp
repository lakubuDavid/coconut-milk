#include "log.h"
#include "debug.h"

namespace coconut::modules {

void init_log(sol::state& lua, ThreadKind kind) {
  (void)kind;  // thread-safe — same implementation on both threads

  sol::table coconut = lua["coconut"].get_or_create<sol::table>();
  coconut.set_function("log",   [](const std::string& msg) { debug::log(msg); });
  coconut.set_function("info",  [](const std::string& msg) { debug::info(msg); });
  coconut.set_function("warn",  [](const std::string& msg) { debug::warn(msg); });
  coconut.set_function("error", [](const std::string& msg) { debug::error(msg); });
}

} // namespace coconut::modules
