#include "hotreload.h"
#include "app.h"
#include "debug.h"
#include "../hotreload.h"  // C++ hotreload namespace

namespace coconut::modules {

void init_hotreload(sol::state& lua, ThreadKind kind) {
  sol::table coconut = lua["coconut"].get_or_create<sol::table>();

  if (kind == ThreadKind::Main) {
    // On main thread, lua["coconut"]["_app"] must be set by caller.
    coconut.set_function("hotreload", [&lua]() -> bool {
      sol::object appObj = lua["coconut"]["_app"];
      if (!appObj.valid()) return false;
      auto* app = appObj.as<coconut::App*>();
      if (!app || !app->configs) return false;
      coconut::hotreload::trigger(app, app->configs);
      return true;
    });
  } else {
    coconut.set_function("hotreload", []() -> bool {
      debug::warn("[bg] coconut.hotreload requires the main thread");
      return false;
    });
  }
}

} // namespace coconut::modules
