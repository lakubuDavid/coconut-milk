#include "bg_stubs.h"
#include "debug.h"

namespace coconut::modules {

void init_bg_stubs(sol::state& lua, ThreadKind kind) {
  if (kind == ThreadKind::Main) return;  // these are bg-only

  sol::table coconut = lua["coconut"].get_or_create<sol::table>();

  coconut.set_function("views", [&lua]() -> sol::table {
    return lua.create_table();
  });

  coconut.set_function("config", [](sol::object ctx) -> sol::object {
    return ctx;
  });

  coconut.set_function("events", [](sol::object) { });

  coconut.set_function("__registerPlatformKeybind", [](sol::table) -> bool {
    debug::warn("[bg] __registerPlatformKeybind requires the main thread");
    return false;
  });
}

} // namespace coconut::modules
