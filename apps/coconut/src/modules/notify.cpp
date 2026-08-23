#include "notify.h"
#include "../packages/notify.h"  // C++ notify namespace
#include "debug.h"
#include "forward.h"

namespace coconut::modules {

  void init_notify(sol::state& lua, ThreadKind kind) {
    sol::table coconut = lua["coconut"].get_or_create<sol::table>();

    if (kind == ThreadKind::Main) {
      coconut.set_function("notify", [](const std::string& title, const std::string& body) -> bool {
        return coconut::notify::notify(title, body);
      });
    } else {
      coconut.set_function("notify", [](const std::string& title, const std::string& body) -> bool {
        // Forward onto the main run loop and block until delivered.
        return forwardToMain([&]() -> bool { return coconut::notify::notify(title, body); });
      });
    }
  }

}  // namespace coconut::modules
