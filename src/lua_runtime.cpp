#include "lua_runtime.h"

#include <sol/state.hpp>
#include <sol/table.hpp>

namespace coconut::lua {

Runtime *create(Config *cfg, CoconutContext *ctx) {
  auto runtime = new Runtime{.configs = cfg, .context = ctx, .lua_state = nullptr};

  runtime->lua_state = new sol::state();
  runtime->lua_state->open_libraries(sol::lib::base, sol::lib::package, sol::lib::io, sol::lib::os,
                                     sol::lib::table, sol::lib::string, sol::lib::math);

  _bindCoconut(runtime);
  _bindViewClass(runtime);
  _bindUserType(runtime);

  return runtime;
}

void _bindCoconut(Runtime *runtime) {
  sol::table coconut = (*runtime->lua_state)["coconut"].get_or_create<sol::table>();
  runtime->lua_state->set("coconut", coconut);
}

void _bindViewClass(Runtime *runtime) {
  sol::table viewClass = (*runtime->lua_state)["View"].get_or_create<sol::table>();
  runtime->lua_state->set("View", viewClass);
}

void _bindUserType(Runtime *runtime) {
  runtime->lua_state->new_usertype<CoconutContext>(
    "CoconutContext",
    "setBrowser", &CoconutContext::setBrowser,
    "setWindowSize", &CoconutContext::setWindowSize,
    "setInitialView", &CoconutContext::setInitialView,
    "show", &CoconutContext::show,
    "reload", &CoconutContext::reload,
    "close", &CoconutContext::close,
    "bind", &CoconutContext::bind,
    "emit", &CoconutContext::emit,
    "emit_sync", &CoconutContext::emit_sync
  );

  runtime->lua_state->set("ctx", runtime->context);
}

void destroy(Runtime *runtime) {
  if (runtime == nullptr) {
    return;
  }

  delete runtime->lua_state;
  delete runtime;
}

} // namespace coconut::lua
