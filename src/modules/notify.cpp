#include "notify.h"
#include "debug.h"
#include "../packages/notify.h"  // C++ notify namespace

namespace coconut::modules {

void init_notify(sol::state& lua, ThreadKind kind) {
  sol::table coconut = lua["coconut"].get_or_create<sol::table>();

  if (kind == ThreadKind::Main) {
    coconut.set_function("notify",
        [](const std::string& title, const std::string& body) -> bool {
          return coconut::notify::notify(title, body);
        });
  } else {
    coconut.set_function("notify",
        [](const std::string&, const std::string&) -> bool {
          debug::warn("[bg] coconut.notify requires the main thread");
          return false;
        });
  }
}

} // namespace coconut::modules
