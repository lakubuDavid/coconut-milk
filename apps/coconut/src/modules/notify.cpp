#include "notify.h"
#include <sol/state.hpp>
#include <sol/table.hpp>
#include <string>

#include "forward.h"
#include "modules/thread_kind.h"
#include "platform/darwin/notify.h"

namespace coconut::modules {

  void init_notify(sol::state& lua, ThreadKind kind) {
    sol::table coconut = lua["coconut"].get_or_create<sol::table>();

    if (kind == ThreadKind::Main) {
      coconut.set_function("notify", [](const std::string& title, const std::string& body) -> bool {
        return notify::platformNotify(title, body);
      });
    } else {
      coconut.set_function("notify", [](const std::string& title, const std::string& body) -> bool {
        // Forward onto the main run loop and block until delivered.
        return forwardToMain([&]() -> bool { return notify::platformNotify(title, body); });
      });
    }
  }

}  // namespace coconut::modules
